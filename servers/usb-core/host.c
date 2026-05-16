#include <mosstd.h>
#include <libsys/ipc.h>
#include <signals.h>
#include <ipc.h>
#include <cap.h>
#include <devman.h>
#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>
#include <string.h>
#include "usb-core.h"

#define USB_MAX_HOSTS 4
#define EP_CTX_PER_DEV 31


typedef struct usb_device {
    uint8_t     host_id;
    uint8_t     slot_id;
    uint8_t     address;
    uint8_t     speed;
    uint8_t     port;
    uint8_t     state;
#define USB_DEV_STATE_DEFAULT    1
#define USB_DEV_STATE_ADDRESSED  2
#define USB_DEV_STATE_CONFIGURED 3
    uint8_t     parent_slot;    /* 0 = root-attached */
    uint8_t     hub_port;       /* downstream port on parent hub (1-based) */
    uint32_t    route_string;

    usb_device_descriptor_t dev_desc;
    usb_config_descriptor_t *config_desc;
} usb_device_t;

typedef struct usb_port
{
    uint8_t		maj_rev;
	uint8_t		min_rev;
    uint8_t     state;
    uint8_t     state_change;
    usb_device_t  *device;
} usb_port_t;


typedef struct usb_host {
    cap_id_t    host_cap;
    cap_id_t    buf_cap;
    void *      buf;   
    size_t      buf_size;
    uint32_t    max_ports;
    usb_port_t  *ports;
} usb_host_t;

usb_host_t *hosts[USB_MAX_HOSTS];

/* Indexed by [host_id-1][slot_id]; slot_id 0 unused (xHCI slot IDs start at 1) */
usb_device_t *devices[USB_MAX_HOSTS][256];

// Pull-based class-driver bind queue. usb-core enqueues devices here; class
// drivers fetch them via IPC_CLASS_POLL_BIND filtered by class_id. Direct
// ipc_call from usb-core to a class driver would deadlock — kernel has no
// IPC queue.
#define MAX_PENDING_BINDS 8
typedef struct pending_class_bind {
    uint8_t  class_id;      // USB_CLASS_ID_*
    uint8_t  slot_id;
    uint8_t  host_id;
    uint8_t  primary_ep_id; // int-IN ep for hub/HID; 0 if none
    uint8_t  speed;
    uint32_t route_string;
    cap_id_t buf_cap;
} pending_class_bind_t;

static pending_class_bind_t pending_binds[MAX_PENDING_BINDS];
static int                  n_pending_binds = 0;

extern pid_t pid;

volatile uint64_t event_flags = 0;
#define USB_EVENT_PORT_CHANGE   (1 << 0)
#define USB_EVENT_DEVICE_ADDED  (1 << 1)
#define USB_EVENT_DEVICE_REMOVED (1 << 2)

void dump_usb_device_config(usb_device_t *dev)
{
    // Parse interfaces and endpoints
    uint8_t *ptr = (uint8_t *)dev->config_desc + sizeof(usb_config_descriptor_t);
    uint16_t remaining = dev->config_desc->wTotalLength - sizeof(usb_config_descriptor_t);
    
    while (remaining >= 2) {  // Need at least length and type
        uint8_t desc_length = ptr[0];
        uint8_t desc_type = ptr[1];
        
        if (desc_length < 2 || desc_length > remaining) {
            break;  // Invalid descriptor
        }
        
        if (desc_type == USB_DT_INTERFACE) {
            usb_interface_descriptor_t *iface_desc = (usb_interface_descriptor_t *)ptr;
            debug("[USB-CORE] Interface %u: Class=0x%02x, SubClass=0x%02x, Protocol=0x%02x, Endpoints=%u\n",
                    iface_desc->bInterfaceNumber, iface_desc->bInterfaceClass,
                    iface_desc->bInterfaceSubClass, iface_desc->bInterfaceProtocol,
                    iface_desc->bNumEndpoints);
        } else if (desc_type == USB_DT_ENDPOINT) {
            usb_endpoint_descriptor_t *ep_desc = (usb_endpoint_descriptor_t *)ptr;
            uint8_t ep_addr = ep_desc->bEndpointAddress;
            uint8_t ep_num = ep_addr & 0x0F;

            uint8_t ep_type = ep_desc->bmAttributes & 0x03;
            
            debug("[USB-CORE] Endpoint 0x%02x: Type=%s, MaxPacket=%u, Interval=%u\n",
                    ep_addr,
                    ep_type == USB_ENDPOINT_XFER_BULK ? "Bulk" :
                    ep_type == USB_ENDPOINT_XFER_INT ? "Interrupt" :
                    ep_type == USB_ENDPOINT_XFER_ISOC ? "Isochronous" : "Control",
                    ep_desc->wMaxPacketSize, ep_desc->bInterval);
        }    
        ptr += desc_length;
        remaining -= desc_length;
        
    }
}

int usb_parce_endpoints(void *cfg_desc, usb_ep_desc_t *ep_descs, size_t max_eps)
{
    size_t ep_count = 0;

    // Parse interfaces and endpoints
    uint8_t *ptr = (uint8_t *)cfg_desc + sizeof(usb_config_descriptor_t);
    uint8_t *end = (uint8_t *)cfg_desc + ((usb_config_descriptor_t *)cfg_desc)->wTotalLength;
    
    while (ptr < end && ep_count < max_eps) {  // Need at least length and type
        uint8_t desc_length = ptr[0];
        uint8_t desc_type = ptr[1];
        
        if (desc_length < 2 || ptr + desc_length > end) {
            break;  // Invalid descriptor
        }
        if (desc_type == USB_DT_INTERFACE) {
                usb_interface_descriptor_t *iface_desc = (usb_interface_descriptor_t *)ptr;
                debug("[XHCI] Interface %u: Class=0x%02x, SubClass=0x%02x, Protocol=0x%02x, Endpoints=%u\n",
                      iface_desc->bInterfaceNumber, iface_desc->bInterfaceClass,
                      iface_desc->bInterfaceSubClass, iface_desc->bInterfaceProtocol,
                      iface_desc->bNumEndpoints);
        } else if (desc_type == USB_DT_ENDPOINT) {
            usb_endpoint_descriptor_t *ep_desc = (usb_endpoint_descriptor_t *)ptr;
            uint8_t ep_addr = ep_desc->bEndpointAddress;
            debug("[USB-CORE] Parsing Endpoint 0x%02x: Type=%s, MaxPacket=%u, Interval=%u\n",
                  ep_addr,
                  (ep_desc->bmAttributes & 0x03) == USB_ENDPOINT_XFER_BULK ? "Bulk" :
                  (ep_desc->bmAttributes & 0x03) == USB_ENDPOINT_XFER_INT ? "Interrupt" :
                  (ep_desc->bmAttributes & 0x03) == USB_ENDPOINT_XFER_ISOC ? "Isochronous" : "Control",
                  ep_desc->wMaxPacketSize, ep_desc->bInterval);
            ep_descs[ep_count].ep_num = ep_addr & 0x0F;
            ep_descs[ep_count].ep_dir = (ep_addr & USB_DIR_MASK) ? USB_DIR_IN : USB_DIR_OUT;
            ep_descs[ep_count].type   = USB_ENDPOINT_XFERTYPE(ep_desc->bmAttributes);
            ep_descs[ep_count].max_packet = USB_ENDPOINT_MAXP(ep_desc->wMaxPacketSize);
            ep_descs[ep_count].interval   = ep_desc->bInterval;

            ep_count++;
        }    
        ptr += desc_length;       
    }

    return ep_count;
}

void handle_host_register(uint64_t sender, uint64_t info)
{
    int ret;

    usb_host_t *host = (usb_host_t *)malloc(sizeof(usb_host_t));
    if (!host) {
        debug("[USB_CORE] Cannot allocate host structure\n");
        return;
    }

    host->host_cap = ipc_get_cap(0);
    host->buf_cap  = ipc_get_cap(1);
    host->buf_size = ipc_getMR(2);
    host->max_ports = ipc_getMR(3);
    host->ports = (usb_port_t *)malloc(sizeof(usb_port_t) * host->max_ports);
    if (!host->ports) {
        debug("[USB_CORE] Cannot allocate host ports structure\n");
        free(host);
        ret = -ENOMEM;
        goto reply_error;
    }
    memset(host->ports, 0, sizeof(usb_port_t) * host->max_ports);

    host->buf = ipc_shm_attach(host->buf_cap, NULL, 0);
    if (!host->buf) {
        debug("[USB_CORE] Cannot attach host buffer shm\n");
        free(host);
        ret = -ERR_MAP_FAIL; 
        goto reply_error;
    }

    debug("[USB_CORE] Registered host cap 0x%lX from sender 0x%lX\n", host->host_cap, sender);
    for (int i = 0; i < USB_MAX_HOSTS; i++) {
        if (!hosts[i]) {
            hosts[i] = host;
            ipc_setMR(0, i + 1); // Host ID starts from 1
            ipc_setMR(1, pid);
            msg_info_t ret_info = msginfo_word_new(0, 2, 0, 0);
            ipc_reply(ret_info);
            return;
        }
    }
    debug("[USB_CORE] No space for new host\n");
    free(host);
    ret = -ENOSPC;

reply_error:
    ipc_setMR(0, ret); 
    msg_info_t ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);    
}

int usb_device_control_transfer(usb_device_t *dev, usb_control_request_t *req, void *data, size_t data_len)
{
    if (dev->host_id == 0 || dev->host_id > USB_MAX_HOSTS) {
        debug("[USB_CORE] Invalid host ID %d for device transfer\n", dev->host_id);
        return -EINVAL;
    }
    usb_host_t *host = hosts[dev->host_id - 1];
    if (!host) {
        debug("[USB_CORE] No host structure for host ID %d\n", dev->host_id);
        return -EINVAL;
    }

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;

    // Prepare IPC message to host
    cmd->cmd_type = IPC_HOST_CTRL_XFER;
    cmd->ctrl.slot_id = dev->slot_id;
    memcpy(cmd->ctrl.setup_pkt, req, sizeof(usb_control_request_t));
    cmd->ctrl.data_len = data_len;
    cmd->ctrl.data_buf = (uint64_t)(host->buf);

    msg_info_t info = msginfo_word_new(0, sizeof(usb_host_cmd_t)/8, 0, 0);

    info = ipc_call(host->host_cap, info);

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Control transfer failed on host ID %d slot %d\n", dev->host_id, dev->slot_id);
        return ret;
    }

    // Copy data back if any
    if (data_len > 0 && data) {
        memcpy(data, host->buf, data_len);
    }

    return ret;
}

void usb_get_device_descriptors(usb_device_t *dev)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DT_DEVICE << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(usb_device_descriptor_t),
    };
    usb_device_descriptor_t dev_desc;
    int ret = usb_device_control_transfer(dev, &req, &dev_desc, sizeof(dev_desc));
    if (ret < 0) {
        debug("[USB_CORE] Failed to get device descriptor for host %d slot %d\n", dev->host_id, dev->slot_id);
        return; 
    }
    memcpy(&dev->dev_desc, &dev_desc, sizeof(usb_device_descriptor_t));
    debug("[USB_CORE] Device Descriptor for host %d slot %d: Class 0x%02X SubClass 0x%02X Protocol 0x%02X VID 0x%04X PID 0x%04X\n",
          dev->host_id, dev->slot_id, dev->dev_desc.bDeviceClass, dev->dev_desc.bDeviceSubClass, dev->dev_desc.bDeviceProtocol,
          dev->dev_desc.idVendor, dev->dev_desc.idProduct);

}

uint8_t usb_get_device_max_packet(usb_device_t *dev)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DT_DEVICE << 8) | 0,
        .wIndex = 0,
        .wLength = 8,
    };
    usb_device_descriptor_t dev_desc;
    int ret = usb_device_control_transfer(dev, &req, &dev_desc, 8);
    if (ret < 0) {
        debug("[USB_CORE] Failed to get device descriptor for host %d slot %d\n", dev->host_id, dev->slot_id);
        return 0; 
    }

    return dev_desc.bMaxPacketSize0;
}

void usb_get_device_configuration(usb_device_t *dev)
{
    usb_config_descriptor_t config_desc;
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DT_CONFIG << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(usb_config_descriptor_t),
    };
    int ret = usb_device_control_transfer(dev, &req, &config_desc, sizeof(config_desc));
    if (ret < 0) {
        debug("[USB_CORE] Failed to get configuration descriptor for host %d slot %d\n", dev->host_id, dev->slot_id);
        return;
    }
    debug("[USB_CORE] Configuration Descriptor for host %d slot %d: Total Length %u Num Interfaces %u\n",
          dev->host_id, dev->slot_id, config_desc.wTotalLength, config_desc.bNumInterfaces);

    dev->config_desc = (usb_config_descriptor_t *)malloc(config_desc.wTotalLength);
    if (!dev->config_desc) {
        debug("[USB_CORE] Cannot allocate memory for full configuration descriptor\n");
        return;
    }
    req.wLength = config_desc.wTotalLength;
    ret = usb_device_control_transfer(dev, &req, dev->config_desc, config_desc.wTotalLength);
    if (ret < 0) {
        debug("[USB_CORE] Failed to get full configuration descriptor for host %d slot %d\n", dev->host_id, dev->slot_id);
        free(dev->config_desc);
        dev->config_desc = NULL;
        return;
    }
    debug("[USB_CORE] Retrieved full configuration descriptor for host %d slot %d\n",
          dev->host_id, dev->slot_id);

}

// TODO: dead — xHCI owns SET_ADDRESS via Address Device command; delete when confirmed
// int usb_set_device_address(usb_device_t *dev, uint8_t address)
// {
//     usb_control_request_t req = {
//         .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
//         .bRequest = USB_REQ_SET_ADDRESS,
//         .wValue = address,
//         .wIndex = 0,
//         .wLength = 0,
//     };
//     int ret = usb_device_control_transfer(dev, &req, NULL, 0);
//     if (ret < 0) {
//         debug("[USB_CORE] Failed to set device address %d for host %d slot %d\n", address, dev->host_id, dev->slot_id);
//         return ret;
//     }
//     dev->address = address;
//     debug("[USB_CORE] Set device address %d for host %d slot %d\n", address, dev->host_id, dev->slot_id);
//     return 0;
// }

int usb_set_device_configuration(usb_device_t *dev, uint8_t config_value)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_SET_CONFIGURATION,
        .wValue = config_value,
        .wIndex = 0,
        .wLength = 0,
    };
    int ret = usb_device_control_transfer(dev, &req, NULL, 0);
    if (ret < 0) {
        debug("[USB_CORE] Failed to set device configuration %d for host %d slot %d\n", config_value, dev->host_id, dev->slot_id);
        return ret;
    }
    debug("[USB_CORE] Set device configuration %d for host %d slot %d\n", config_value, dev->host_id, dev->slot_id);
    return 0;
}

uint8_t usb_get_configuration(usb_device_t *dev)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_CONFIGURATION,
        .wValue = 0,
        .wIndex = 0,
        .wLength = 1,
    };
    uint8_t config_value;
    int ret = usb_device_control_transfer(dev, &req, &config_value, 1);
    if (ret < 0) {
        debug("[USB_CORE] Failed to get device configuration for host %d slot %d\n", dev->host_id, dev->slot_id);
        return ret;
    }
    debug("[USB_CORE] Got device configuration for host %d slot %d => %d\n", dev->host_id, dev->slot_id, config_value);
    return config_value;
}

uint8_t usb_get_interface(usb_device_t *dev, uint8_t interface_number)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
        .bRequest = USB_REQ_GET_INTERFACE,
        .wValue = 0,
        .wIndex = interface_number,
        .wLength = 1,
    };
    uint8_t alt_setting;
    int ret = usb_device_control_transfer(dev, &req, &alt_setting, 1);
    if (ret < 0) {
        debug("[USB_CORE] Failed to get interface %d for host %d slot %d\n", interface_number, dev->host_id, dev->slot_id);
        return ret;
    }
    debug("[USB_CORE] Got interface %d for host %d slot %d => alt setting %d\n", interface_number, dev->host_id, dev->slot_id, alt_setting);
    return alt_setting;
}

int usb_configure_endpoints(usb_device_t *dev, uint8_t ep_nums, usb_ep_desc_t *ep_desc)
{
    if (dev->host_id == 0 || dev->host_id > USB_MAX_HOSTS) {
        debug("[USB_CORE] Invalid host ID %d for device endpoint configuration\n", dev->host_id);
        return -EINVAL;
    }
    usb_host_t *host = hosts[dev->host_id - 1];
    if (!host) {
        debug("[USB_CORE] No host structure for host ID %d\n", dev->host_id);
        return -EINVAL;
    }

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;

    // Prepare IPC message to host
    cmd->cmd_type = IPC_HOST_CONFIG_EP;
    cmd->config_ep.slot_id = dev->slot_id;
    cmd->config_ep.ep_nums = ep_nums;

    memcpy(cmd->config_ep.ep_desc, ep_desc, sizeof(usb_ep_desc_t) * ep_nums);

    msg_info_t info = msginfo_word_new(0, (sizeof(usb_host_cmd_t) + (sizeof(usb_ep_desc_t) * ep_nums))/8, 0, 0);

    info = ipc_call(host->host_cap, info);

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Endpoint configuration failed on host ID %d slot %d\n", dev->host_id, dev->slot_id);
        return ret;
    }

    return ipc_getMR(0);
}

uint8_t usb_host_enable_slot(usb_host_t *host)
{
    ipc_setMR(0, IPC_HOST_ENABLE_SLOT);

    msg_info_t info = msginfo_word_new(0, 1, 0, 0);

    info = ipc_call(host->host_cap, info);

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Enable slot failed on host\n");
        return ret;
    }

    return ipc_getMR(0);
}

int usb_host_new_device(usb_device_t *dev, uint8_t port)
{
    int ret;
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));

    cmd->cmd_type = IPC_HOST_NEW_DEVICE;
    cmd->new_dev.slot_id      = dev->slot_id;
    cmd->new_dev.port         = port;
    cmd->new_dev.speed        = dev->speed;
    cmd->new_dev.parent_slot  = dev->parent_slot;
    cmd->new_dev.hub_port     = dev->hub_port;
    cmd->new_dev.route_string = dev->route_string;
    /* TT hub slot for FS/LS devices connected through HS hub */
    cmd->new_dev.tt_slot = (dev->speed == USB_SPEED_FULL || dev->speed == USB_SPEED_LOW)
                            ? dev->parent_slot : 0;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd)/8, 0, 0);
    info = ipc_call(hosts[dev->host_id-1]->host_cap, info);
    ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Creating new device failed on host\n");
        return ret;
    }    

    if (cmd->cmd_type != IPC_NEW_DEVICE)
        return ipc_getMR(0);

    dev->speed = cmd->new_dev.speed;
    dev->state = cmd->new_dev.state;

    return SUCCESS;
}

int usb_host_stop_ep(usb_device_t *dev, uint8_t ep_id)
{
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));

    cmd->cmd_type         = IPC_HOST_STOP_EP;
    cmd->stop_ep.slot_id  = dev->slot_id;
    cmd->stop_ep.ep_id    = ep_id;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd)/8, 0, 0);
    info = ipc_call(hosts[dev->host_id - 1]->host_cap, info);
    /* best-effort: ignore failure and proceed with teardown */
    return ipc_getMR(0);
}

int usb_host_disable_slot(usb_device_t *dev)
{
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));

    cmd->cmd_type        = IPC_HOST_DISABLE_SLOT;
    cmd->new_dev.slot_id = dev->slot_id;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd)/8, 0, 0);
    info = ipc_call(hosts[dev->host_id - 1]->host_cap, info);
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Disable slot failed for slot %d\n", dev->slot_id);
        return ret;
    }
    return ipc_getMR(0);
}

int usb_host_update_ep0_mps(usb_device_t *dev, uint16_t mps)
{
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));

    cmd->cmd_type = IPC_HOST_UPDATE_EP0_MPS;
    cmd->update_ep0_mps.slot_id = dev->slot_id;
    cmd->update_ep0_mps.mps     = mps;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd)/8, 0, 0);
    info = ipc_call(hosts[dev->host_id - 1]->host_cap, info);
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Update EP0 mps failed for slot %d\n", dev->slot_id);
        return ret;
    }
    return ipc_getMR(0);
}

static int usb_clear_endpoint_halt(usb_device_t *dev, uint8_t ep_addr)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
        .bRequest = USB_REQ_CLEAR_FEATURE,
        .wValue = 0,   /* ENDPOINT_HALT feature selector */
        .wIndex = ep_addr,
        .wLength = 0,
    };
    return usb_device_control_transfer(dev, &req, NULL, 0);
}

int usb_host_address_device(usb_device_t *dev)
{
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));

    cmd->cmd_type = IPC_HOST_ADDRESS_DEVICE;
    cmd->new_dev.slot_id = dev->slot_id;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd)/8, 0, 0);
    info = ipc_call(hosts[dev->host_id - 1]->host_cap, info);
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0) {
        debug("[USB_CORE] Address device failed for slot %d\n", dev->slot_id);
        return ret;
    }
    return ipc_getMR(0);
}

/* Submit a bulk/interrupt/isochronous transfer.
 * ep_id: xHCI ep_id (EP_N_OUT = N*2, EP_N_IN = N*2+1).
 * dir: USB_DIR_IN or USB_DIR_OUT.
 * data: caller buffer; copied to/from host->buf as staging area.
 * Returns 0 on success, -EBUSY on STALL (cleared by CLEAR_FEATURE), negative on error. */
int usb_host_xfer_submit(usb_device_t *dev, uint8_t ep_id, uint8_t dir,
                          void *data, uint32_t len, uint32_t *actual_out)
{
    if (dev->host_id == 0 || dev->host_id > USB_MAX_HOSTS) return -EINVAL;
    usb_host_t *host = hosts[dev->host_id - 1];
    if (!host) return -EINVAL;
    if (!len || len > host->buf_size) return -EINVAL;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));

    cmd->cmd_type    = IPC_HOST_XFER_SUBMIT;
    cmd->xfer.slot_id = dev->slot_id;
    cmd->xfer.ep_id   = ep_id;
    cmd->xfer.dir     = dir;
    cmd->xfer.len     = len;

    /* For OUT: stage data into shared buffer before the call */
    if (dir != USB_DIR_IN && data && len)
        memcpy(host->buf, data, len);

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(host->host_cap, info);

    int ret      = (int)ipc_getMR(0);
    uint32_t actual = (uint32_t)ipc_getMR(1);

    if (ret == -EBUSY) {
        /* STALL: xHCI already issued Reset Endpoint; clear the device halt */
        uint8_t ep_num  = ep_id / 2;
        uint8_t ep_addr = ep_num | ((ep_id & 1) ? USB_DIR_IN : 0);
        debug("[USB_CORE] STALL on slot %d ep_id %d — sending CLEAR_FEATURE ep_addr 0x%02x\n",
              dev->slot_id, ep_id, ep_addr);
        usb_clear_endpoint_halt(dev, ep_addr);
        return -EBUSY;
    }

    if (ret < 0) {
        if (ret != -ETIMEOUT) {
            debug("[USB_CORE] XFER_SUBMIT failed slot %d ep_id %d ret %d\n",
                  dev->slot_id, ep_id, ret);
        }
        return ret;
    }

    /* For IN: copy result from shared buffer to caller */
    if (dir == USB_DIR_IN && data && actual)
        memcpy(data, host->buf, actual);

    if (actual_out)
        *actual_out = actual;

    return 0;
}

// Return the xHCI ep_id of the first interrupt-IN endpoint, or 0 if none.
static uint8_t find_int_in_ep(usb_ep_desc_t *eps, int count)
{
    for (int i = 0; i < count; i++) {
        if (eps[i].type == USB_ENDPOINT_XFER_INT && eps[i].ep_dir == USB_DIR_IN)
            return eps[i].ep_num * 2 + 1; // xHCI ep_id: ep_num*2+1 for IN
    }
    return 0;
}

// Return bInterfaceClass of the first interface in cfg_desc, or 0 on failure.
static uint8_t usb_first_interface_class(usb_config_descriptor_t *cfg)
{
    uint8_t *ptr = (uint8_t *)cfg + sizeof(usb_config_descriptor_t);
    uint8_t *end = (uint8_t *)cfg + cfg->wTotalLength;
    while (ptr + 2 <= end) {
        uint8_t len  = ptr[0];
        uint8_t type = ptr[1];
        if (len < 2 || ptr + len > end) break;
        if (type == USB_DT_INTERFACE)
            return ((usb_interface_descriptor_t *)ptr)->bInterfaceClass;
        ptr += len;
    }
    return 0;
}

// Enqueue a device for a class driver to pick up via IPC_CLASS_POLL_BIND.
static void usb_enqueue_bind(usb_device_t *dev, uint8_t class_id, uint8_t primary_ep_id)
{
    if (n_pending_binds >= MAX_PENDING_BINDS) {
        debug("[USB_CORE] pending_binds full — slot %d dropped\n", dev->slot_id);
        return;
    }
    usb_host_t *host = hosts[dev->host_id - 1];
    pending_class_bind_t *p = &pending_binds[n_pending_binds++];
    p->class_id      = class_id;
    p->slot_id       = dev->slot_id;
    p->host_id       = dev->host_id;
    p->primary_ep_id = primary_ep_id;
    p->speed         = dev->speed;
    p->route_string  = dev->route_string;
    p->buf_cap       = host->buf_cap;
    debug("[USB_CORE] Bind queued class=%u slot=%d ep=%u (pending=%d)\n",
          class_id, dev->slot_id, primary_ep_id, n_pending_binds);
}

/* Fetch descriptors, configure endpoints, register device in the slot table.
 * Called after the device has been fully addressed (BSR=false). */
static int usb_device_setup(usb_device_t *dev)
{
    int ret;

    if (devices[dev->host_id - 1][dev->slot_id]) {
        debug("[USB_CORE] Slot %d already registered — skipping setup\n", dev->slot_id);
        return -EEXIST;
    }
    devices[dev->host_id - 1][dev->slot_id] = dev;

    usb_get_device_descriptors(dev);
    usb_get_device_configuration(dev);

    if (!dev->config_desc) {
        debug("[USB_CORE] No config descriptor for slot %d\n", dev->slot_id);
        return -EIO;
    }

    ret = usb_set_device_configuration(dev, dev->config_desc->bConfigurationValue);
    if (ret < 0)
        debug("[USB_CORE] SET_CONFIGURATION failed for slot %d\n", dev->slot_id);

    usb_ep_desc_t ep_descs[EP_CTX_PER_DEV];
    int ep_count = usb_parce_endpoints(dev->config_desc, ep_descs, EP_CTX_PER_DEV);
    debug("[USB_CORE] %d endpoints for slot %d\n", ep_count, dev->slot_id);

    if (ep_count > 0) {
        ret = usb_configure_endpoints(dev, ep_count, ep_descs);
        if (ret < 0)
            debug("[USB_CORE] Configure endpoints failed for slot %d\n", dev->slot_id);
        else
            dev->state = ret;
    }

    // Determine effective class (fall back to first interface if device-level is 0)
    uint8_t cls = dev->dev_desc.bDeviceClass;
    if (cls == 0)
        cls = usb_first_interface_class(dev->config_desc);

    debug("[USB_CORE] Device ready host %d slot %d class 0x%02x state %d\n",
          dev->host_id, dev->slot_id, cls, dev->state);

    switch (cls) {
    case USB_CLASS_HUB:
        usb_enqueue_bind(dev, USB_CLASS_ID_HUB, find_int_in_ep(ep_descs, ep_count));
        break;
    case USB_CLASS_HID:
        usb_enqueue_bind(dev, USB_CLASS_ID_HID, find_int_in_ep(ep_descs, ep_count));
        break;
    default:
        debug("[USB_CORE] slot %d class 0x%02x has no driver\n", dev->slot_id, cls);
        break;
    }

    return 0;
}

/* Tear down a device: stop all EPs, disable slot, free memory. */
static void usb_device_teardown(usb_device_t *dev)
{
    if (!dev) return;

    debug("[USB_CORE] Tearing down host %d slot %d\n", dev->host_id, dev->slot_id);

    /* Stop all non-EP0 endpoints (EP IDs 2..31 = EPs 1..15 in both directions).
     * EP0 (ep_id=1) is torn down implicitly by Disable Slot. */
    for (uint8_t ep_id = 2; ep_id <= 31; ep_id++)
        usb_host_stop_ep(dev, ep_id);

    usb_host_disable_slot(dev);

    /* Free local state */
    if (dev->config_desc) {
        free(dev->config_desc);
        dev->config_desc = NULL;
    }

    if (dev->slot_id && dev->host_id)
        devices[dev->host_id - 1][dev->slot_id] = NULL;
}

// void handle_new_dev(uint64_t sender, uint64_t info)
// {
//     int ret;
//     msg_info_t ret_info;

//     usb_core_cmd_t *cmd = (usb_core_cmd_t *)get_ipc_buffer()->msg;
//     uint8_t host_id = cmd->new_dev.host_id;
//     uint8_t slot_id = cmd->new_dev.slot_id;

//     debug("[USB_CORE] New device connected on host %d slot %d\n", host_id, slot_id);

//     usb_device_t *dev = (usb_device_t *)malloc(sizeof(usb_device_t));
//     if (!dev) {
//         debug("[USB_CORE] Cannot allocate device structure\n");
//         ret = -ENOMEM;
//         goto reply_error;
//     }

//     if (host_id > USB_MAX_HOSTS || !hosts[host_id - 1]) {
//         debug("[USB_CORE] Invalid host ID %d for new device\n", host_id);
//         free(dev);
//         ret = -EINVAL;
//         goto reply_error;
//     }
//     dev->host_id = host_id;

//     if (slot_id == 0 || slot_id > 63) {
//         debug("[USB_CORE] Invalid slot ID %d for new device\n", slot_id);
//         free(dev);
//         ret = -EINVAL;
//         goto reply_error;
//     }
//     dev->slot_id = slot_id;

//     if (devices[host_id - 1][slot_id]) {
//         debug("[USB_CORE] Device already exists on host %d slot %d\n", host_id, slot_id);
//         free(dev);
//         ret = -EEXIST;
//         goto reply_error;
//     }
//     dev->address = cmd->new_dev.address;
//     dev->speed   = cmd->new_dev.speed;
//     dev->port    = cmd->new_dev.port;
//     dev->state   = cmd->new_dev.state;
//     devices[host_id - 1][slot_id] = dev;

//     debug("[USB_CORE] Registered new device on host %d slot %d address %d speed %d port %d state %d\n", 
//           host_id, slot_id, dev->address, dev->speed, dev->port, dev->state);

    
//     usb_get_device_descriptors(dev);

//     usb_get_device_configuration(dev);

//     ret = usb_set_device_configuration(dev, dev->config_desc->bConfigurationValue);
//     if (ret < 0) {
//         debug("[USB_CORE] Failed to set device configuration for host %d slot %d\n", dev->host_id, dev->slot_id);
//     }

//     usb_ep_desc_t ep_descs[EP_CTX_PER_DEV];
//     int ep_count = usb_parce_endpoints(dev->config_desc, ep_descs, EP_CTX_PER_DEV);
//     debug("[USB_CORE] Parsed %d endpoints for host %d slot %d\n", ep_count, dev->host_id, dev->slot_id);

//     ret = usb_configure_endpoints(dev, ep_count, ep_descs);
//     if (ret < 0) {
//         debug("[USB_CORE] Failed to configure endpoints for host %d slot %d\n", dev->host_id, dev->slot_id);
//     }

//     dev->state = ret;
//     debug("[USB_CORE] Device on host %d slot %d configured successfully has state %d\n", dev->host_id, dev->slot_id, dev->state);

//     // bind drivers here in future
//     return;

// reply_error:
//     // ipc_setMR(0, ret); 
//     // ret_info = msginfo_word_new(0, 1, 0, 0);
//     // ipc_reply(ret_info);
// }

void handle_host_ipc(uint64_t sender, uint64_t info)
{
    switch (ipc_getMR(1)){
        case IPC_HOST_REGISTER: {
            // debug("[USB_CORE] IPC HOST REGISTER\n");
            handle_host_register(sender, info);
            break;
        }

        // case IPC_DEVICE_CONNECT: {
        //     // debug("[USB_CORE] IPC from DEV\n");
        //     handle_new_dev(sender, info);
        //     break;
        // }

        default:
            break;
    }
}

void handle_port_status_change(uint8_t host_id, uint8_t port_id, uint8_t status)
{
    debug("\x1b[32m[USB_CORE]\x1b[0m Host %d Port %d Status 0x%lX\n", host_id, port_id, status);

    if (host_id == 0 || host_id > USB_MAX_HOSTS) {
        debug("[USB_CORE] Invalid host ID %d for port status change\n", host_id);
        return;
    }
    usb_host_t *host = hosts[host_id - 1];
    if (port_id >= host->max_ports) {
        debug("[USB_CORE] Invalid port ID %d for host %d (max %d)\n", port_id, host_id, host->max_ports);
        return;
    }
    host->ports[port_id].state = status;
    host->ports[port_id].state_change = 1;
}

static void _host_signals_handler(int signum, uint64_t context)
{
    if (signum == SIGNAL_USER_BASE) {
        usb_host_signal_t port_signal;
        port_signal.signal = context;

        handle_port_status_change(port_signal.host_id, port_signal.port_id, port_signal.state);
        if (port_signal.state == USB_PORT_STATE_DISCONNECTED)
            event_flags |= USB_EVENT_DEVICE_REMOVED;
        else
            event_flags |= USB_EVENT_PORT_CHANGE;
    }
}

int usb_device_enumerate(usb_device_t *dev, uint8_t parent_port)
{
    int ret;

    /* 1. Allocate an xHCI slot */
    ret = usb_host_enable_slot(hosts[dev->host_id - 1]);
    if (ret < 0) {
        debug("[USB_CORE] Failed to enable slot for host %d\n", dev->host_id);
        return ret;
    }
    dev->slot_id = ret;
    debug("[USB_CORE] Enabled slot %d for host %d\n", dev->slot_id, dev->host_id);

    /* 2. Init device context + Address Device (BSR=true — don't send SET_ADDRESS yet) */
    ret = usb_host_new_device(dev, parent_port);
    if (ret < 0) {
        debug("[USB_CORE] Failed to init device context on host %d\n", dev->host_id);
        return ret;
    }
    debug("[USB_CORE] Device context initialised slot %u port %u speed %u\n",
          dev->slot_id, dev->port, dev->speed);

    /* 3. Read first 8 bytes of device descriptor to learn EP0 max packet size */
    uint16_t mps = (uint16_t)usb_get_device_max_packet(dev);
    if (!mps)
        return -EIO;
    /* xHCI encodes SS max packet as exponent 9 (2^9=512); normalise to bytes */
    mps = (mps == 9) ? 512 : mps;
    debug("[USB_CORE] EP0 max packet size %u for slot %u\n", mps, dev->slot_id);

    /* 4. Update the EP0 context with the real max packet size */
    ret = usb_host_update_ep0_mps(dev, mps);
    if (ret < 0) {
        debug("[USB_CORE] Failed to update EP0 mps for slot %d\n", dev->slot_id);
        return ret;
    }

    /* 5. Address Device (BSR=false) — xHCI sends SET_ADDRESS to device */
    ret = usb_host_address_device(dev);
    if (ret < 0) {
        debug("[USB_CORE] Failed to address device slot %d\n", dev->slot_id);
        return ret;
    }
    debug("[USB_CORE] Device addressed: slot %u\n", dev->slot_id);

    return 0;
}

int usb_core_init()
{
    for (int i = 0; i < USB_MAX_HOSTS; i++) {
        hosts[i] = NULL;
    }
    for (int h = 0; h < USB_MAX_HOSTS; h++) {
        for (int s = 0; s < 256; s++) {
            devices[h][s] = NULL;
        }
    }

    // set signal handler
    signal_action_t irqhand;
    irqhand.handler = _host_signals_handler;
    signal_action(SIGNAL_USER_BASE, &irqhand);
    return 0;
}

// ── Class driver IPC proxy handlers ─────────────────────────────────────

// class→usb-core: proxy a control transfer
static void handle_class_dev_ctrl_xfer(uint64_t sender, uint64_t info)
{
    msg_info_t ret_info;
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;

    uint8_t  slot_id  = cmd->ctrl.slot_id;
    uint8_t  host_id  = cmd->ctrl.host_id;
    uint32_t data_len = cmd->ctrl.data_len;

    if (host_id == 0 || host_id > USB_MAX_HOSTS || !devices[host_id-1][slot_id]) {
        ipc_setMR(0, -EINVAL);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    usb_device_t *dev = devices[host_id - 1][slot_id];
    usb_control_request_t req;
    memcpy(&req, cmd->ctrl.setup_pkt, 8);

    // Data is in host->buf (class driver wrote there before the call).
    // For IN, result is written back to host->buf.
    int ret = usb_device_control_transfer(dev, &req,
                                          (data_len > 0) ? hosts[host_id-1]->buf : NULL,
                                          data_len);
    ipc_setMR(0, ret);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
}

// class→usb-core: proxy a bulk/interrupt transfer
static void handle_class_dev_xfer_submit(uint64_t sender, uint64_t info)
{
    msg_info_t ret_info;
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;

    uint8_t  slot_id = cmd->xfer.slot_id;
    uint8_t  host_id = cmd->xfer.host_id;
    uint8_t  ep_id   = cmd->xfer.ep_id;
    uint8_t  dir     = cmd->xfer.dir;
    uint32_t len     = cmd->xfer.len;

    if (host_id == 0 || host_id > USB_MAX_HOSTS || !devices[host_id-1][slot_id]) {
        ipc_setMR(0, -EINVAL);
        ipc_setMR(1, 0);
        ipc_reply(msginfo_word_new(0, 2, 0, 0));
        return;
    }

    usb_device_t *dev = devices[host_id - 1][slot_id];
    uint32_t actual = 0;
    int ret = usb_host_xfer_submit(dev, ep_id, dir,
                                   hosts[host_id-1]->buf, len, &actual);
    ipc_setMR(0, ret);
    ipc_setMR(1, actual);
    ret_info = msginfo_word_new(0, 2, 0, 0);
    ipc_reply(ret_info);
}

// hub→usb-core: child device connected on hub port
static void handle_class_port_connect(uint64_t sender, uint64_t info)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;

    uint8_t  hub_slot    = cmd->port_connect.slot_id;
    uint8_t  hub_port    = cmd->port_connect.hub_port;
    uint8_t  speed       = cmd->port_connect.speed;
    uint32_t route_str   = cmd->port_connect.route_string;

    usb_device_t *hub_dev = NULL;
    for (int h = 0; h < USB_MAX_HOSTS && !hub_dev; h++) {
        if (devices[h][hub_slot])
            hub_dev = devices[h][hub_slot];
    }
    if (!hub_dev) {
        debug("[USB_CORE] Hub port connect: hub slot %u not found\n", hub_slot);
        ipc_setMR(0, -EINVAL);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    usb_device_t *child = (usb_device_t *)malloc(sizeof(usb_device_t));
    if (!child) {
        ipc_setMR(0, -ENOMEM);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }
    memset(child, 0, sizeof(*child));
    child->host_id      = hub_dev->host_id;
    child->speed        = speed;
    child->parent_slot  = hub_slot;
    child->hub_port     = hub_port;
    child->route_string = route_str;

    int ret = usb_device_enumerate(child, hub_port);
    if (ret < 0) {
        debug("[USB_CORE] Hub child enumeration failed (hub_slot=%u port=%u): %d\n",
              hub_slot, hub_port, ret);
        free(child);
        ipc_setMR(0, ret);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    ret = usb_device_setup(child);
    if (ret < 0)
        debug("[USB_CORE] Hub child setup failed (%d)\n", ret);

    ipc_setMR(0, ret < 0 ? ret : 0);
    ipc_reply(msginfo_word_new(0, 1, 0, 0));
}

// hub→usb-core: child device disconnected from hub port
static void handle_class_port_disconnect(uint64_t sender, uint64_t info)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    uint8_t hub_slot = cmd->port_disconnect.slot_id;
    uint8_t hub_port = cmd->port_disconnect.hub_port;

    for (int h = 0; h < USB_MAX_HOSTS; h++) {
        for (int s = 1; s < 256; s++) {
            usb_device_t *d = devices[h][s];
            if (d && d->parent_slot == hub_slot && d->hub_port == hub_port) {
                devices[h][s] = NULL;
                usb_device_teardown(d);
                free(d);
                debug("[USB_CORE] Hub child disconnected (hub_slot=%u port=%u)\n",
                      hub_slot, hub_port);
                break;
            }
        }
    }
    ipc_setMR(0, 0);
    ipc_reply(msginfo_word_new(0, 1, 0, 0));
}

// class→usb-core: pull next pending bind matching class_id. slot_id=0 = none.
static void handle_class_poll_bind(uint64_t sender, uint64_t info)
{
    (void)sender; (void)info;
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    uint8_t want_class = cmd->poll_bind.class_id;

    // Find first matching entry
    int found = -1;
    for (int i = 0; i < n_pending_binds; i++) {
        if (pending_binds[i].class_id == want_class) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        ipc_setMR(0, 0);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    pending_class_bind_t *p = &pending_binds[found];
    ipc_setMR(0, p->slot_id);
    ipc_setMR(1, p->host_id);
    ipc_setMR(2, p->primary_ep_id);
    ipc_setMR(3, p->route_string);
    ipc_setMR(4, p->speed);
    ipc_set_cap(0, p->buf_cap);
    msg_info_t reply = msginfo_word_new(0, 5, 1, 0);

    // Remove found entry
    for (int i = found; i < n_pending_binds - 1; i++)
        pending_binds[i] = pending_binds[i + 1];
    n_pending_binds--;

    ipc_reply(reply);
}

void handle_class_ipc(uint64_t sender, uint64_t info)
{
    uint64_t cmd_type = ipc_getMR(1);
    switch (cmd_type) {
        case IPC_CLASS_POLL_BIND:
            handle_class_poll_bind(sender, info);
            break;
        case IPC_CLASS_DEV_CTRL_XFER:
            handle_class_dev_ctrl_xfer(sender, info);
            break;
        case IPC_CLASS_DEV_XFER_SUBMIT:
            handle_class_dev_xfer_submit(sender, info);
            break;
        case IPC_CLASS_PORT_CONNECT:
            handle_class_port_connect(sender, info);
            break;
        case IPC_CLASS_PORT_DISCONNECT:
            handle_class_port_disconnect(sender, info);
            break;
        default:
            debug("[USB_CORE] Unknown class IPC cmd_type %llu\n", cmd_type);
            break;
    }
}

void handle_usb_events(void)
{
    if (event_flags & USB_EVENT_PORT_CHANGE) {
        // Handle port changes
        for (int h = 0; h < USB_MAX_HOSTS; h++) {
            usb_host_t *host = hosts[h];
            if (!host) continue;

            for (int p = 0; p < host->max_ports; p++) {
                if (host->ports[p].state_change) {
                    if (host->ports[p].state == USB_PORT_STATE_CONNECTED) {
                        debug("[USB_CORE] Detected device connection on host %d port %d\n", h + 1, p);
                        // Handle device enumeration here
                        if (host->ports[p].device == NULL) {
                            usb_device_t *dev = (usb_device_t *)malloc(sizeof(usb_device_t));
                            if (!dev) {
                                debug("[USB_CORE] Cannot allocate device structure for host %d port %d\n", h + 1, p);
                                continue;
                            }
                            dev->host_id = h + 1;
                            dev->port = p;
                            dev->state = 0;
                            host->ports[p].device = dev;

                            int ret = usb_device_enumerate(dev, p);
                            if (ret < 0) {
                                debug("[USB_CORE] Failed to enumerate device on host %d port %d\n", h + 1, p);
                                free(dev);
                                host->ports[p].device = NULL;
                            } else {
                                debug("[USB_CORE] Device enumerated successfully on host %d port %d\n", h + 1, p);
                                ret = usb_device_setup(dev);
                                if (ret < 0)
                                    debug("[USB_CORE] Device setup failed (%d) on host %d port %d\n", ret, h + 1, p);
                            }
                        }
                    }
                    host->ports[p].state_change = 0;
                }
            }
        }
        event_flags &= ~USB_EVENT_PORT_CHANGE;
    }

    if (event_flags & USB_EVENT_DEVICE_REMOVED) {
        for (int h = 0; h < USB_MAX_HOSTS; h++) {
            usb_host_t *host = hosts[h];
            if (!host) continue;

            for (int p = 0; p < (int)host->max_ports; p++) {
                if (host->ports[p].state == USB_PORT_STATE_DISCONNECTED &&
                    host->ports[p].device != NULL) {
                    debug("[USB_CORE] Device disconnected on host %d port %d\n", h + 1, p);
                    usb_device_t *dev = host->ports[p].device;
                    host->ports[p].device = NULL;
                    usb_device_teardown(dev);
                    free(dev);
                }
            }
        }
        event_flags &= ~USB_EVENT_DEVICE_REMOVED;
    }
}