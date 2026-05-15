#include <mosstd.h>
#include <libsys/ipc.h>
#include <signals.h>
#include <ipc.h>
#include <cap.h>
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

    usb_device_descriptor_t dev_desc;
    usb_config_descriptor_t *config_desc;
} usb_device_t;

typedef struct usb_port
{
    uint8_t		maj_rev;
	uint8_t		min_rev;
    uint8_t     state;
#define USB_PORT_STATE_DISCONNECTED 0
#define USB_PORT_STATE_CONNECTED    1
#define USB_PORT_STATE_ENABLED      2
#define USB_PORT_STATE_SUSPENDED    3
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



usb_device_t *devices[256];

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

int usb_set_device_address(usb_device_t *dev, uint8_t address)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_SET_ADDRESS,
        .wValue = address,
        .wIndex = 0,
        .wLength = 0,
    };
    int ret = usb_device_control_transfer(dev, &req, NULL, 0);
    if (ret < 0) {
        debug("[USB_CORE] Failed to set device address %d for host %d slot %d\n", address, dev->host_id, dev->slot_id);
        return ret;
    }
    dev->address = address;
    debug("[USB_CORE] Set device address %d for host %d slot %d\n", address, dev->host_id, dev->slot_id);
    return 0;
}

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
    cmd->new_dev.slot_id = dev->slot_id;
    cmd->new_dev.port = port;

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

void handle_new_dev(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_core_cmd_t *cmd = (usb_core_cmd_t *)get_ipc_buffer()->msg;
    uint8_t host_id = cmd->new_dev.host_id;
    uint8_t slot_id = cmd->new_dev.slot_id;

    debug("[USB_CORE] New device connected on host %d slot %d\n", host_id, slot_id);

    usb_device_t *dev = (usb_device_t *)malloc(sizeof(usb_device_t));
    if (!dev) {
        debug("[USB_CORE] Cannot allocate device structure\n");
        ret = -ENOMEM;
        goto reply_error;
    }

    if (host_id > USB_MAX_HOSTS || !hosts[host_id - 1]) {
        debug("[USB_CORE] Invalid host ID %d for new device\n", host_id);
        free(dev);
        ret = -EINVAL;
        goto reply_error;
    }
    dev->host_id = host_id;

    if (slot_id == 0 || slot_id > 63) {
        debug("[USB_CORE] Invalid slot ID %d for new device\n", slot_id);
        free(dev);
        ret = -EINVAL;
        goto reply_error;
    }
    dev->slot_id = slot_id;

    if (devices[(host_id << 6) | slot_id]) {
        debug("[USB_CORE] Device already exists on host %d slot %d 0x%lX\n", host_id, slot_id, (host_id << 6) | slot_id);
        free(dev);
        ret = -EEXIST;
        goto reply_error;
    }   
    dev->address = cmd->new_dev.address;
    dev->speed   = cmd->new_dev.speed;
    dev->port    = cmd->new_dev.port;
    dev->state   = cmd->new_dev.state;
    devices[(host_id << 6) | slot_id] = dev;

    debug("[USB_CORE] Registered new device on host %d slot %d address %d speed %d port %d state %d\n", 
          host_id, slot_id, dev->address, dev->speed, dev->port, dev->state);

    
    usb_get_device_descriptors(dev);

    usb_get_device_configuration(dev);

    ret = usb_set_device_configuration(dev, dev->config_desc->bConfigurationValue);
    if (ret < 0) {
        debug("[USB_CORE] Failed to set device configuration for host %d slot %d\n", dev->host_id, dev->slot_id);
    }

    usb_ep_desc_t ep_descs[EP_CTX_PER_DEV];
    int ep_count = usb_parce_endpoints(dev->config_desc, ep_descs, EP_CTX_PER_DEV);
    debug("[USB_CORE] Parsed %d endpoints for host %d slot %d\n", ep_count, dev->host_id, dev->slot_id);

    ret = usb_configure_endpoints(dev, ep_count, ep_descs);
    if (ret < 0) {
        debug("[USB_CORE] Failed to configure endpoints for host %d slot %d\n", dev->host_id, dev->slot_id);
    }

    dev->state = ret;
    debug("[USB_CORE] Device on host %d slot %d configured successfully has state %d\n", dev->host_id, dev->slot_id, dev->state);

    // bind drivers here in future
    return;

reply_error:
    // ipc_setMR(0, ret); 
    // ret_info = msginfo_word_new(0, 1, 0, 0);
    // ipc_reply(ret_info);
}

void handle_host_ipc(uint64_t sender, uint64_t info)
{
    switch (ipc_getMR(1)){
        case IPC_HOST_REGISTER: {
            // debug("[USB_CORE] IPC HOST REGISTER\n");
            handle_host_register(sender, info);
            break;
        }

        case IPC_DEVICE_CONNECT: {
            // debug("[USB_CORE] IPC from DEV\n");
            handle_new_dev(sender, info);
            break;
        }

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
    if (port_id > host->max_ports) {
        debug("[USB_CORE] Invalid port ID %d for host %d\n", port_id, host_id);
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

        handle_port_status_change(port_signal.host_id, port_signal.port_id, USB_PORT_STATE_CONNECTED);
        event_flags |= USB_EVENT_PORT_CHANGE;
        
    }
}

int usb_device_enumerate(usb_device_t *dev, uint8_t parent_port)
{
    int ret;

    ret = usb_host_enable_slot(hosts[dev->host_id - 1]);
    if (ret < 0) {
        debug("[USB_CORE] Failed to enable slot for host %d\n", dev->host_id);
        return ret;
    }
    dev->slot_id = ret;
    debug("[USB_CORE] Enabled slot %d for host %d\n", dev->slot_id, dev->host_id);

    ret = usb_host_new_device(dev, parent_port);
    if (ret < 0) {
        debug("[USB_CORE] Failed to create new device on host %d\n", dev->host_id);
        return ret;
    }

    debug("[USB_CORE] New device created slot %u port %u speed %u state %u\n", dev->slot_id, dev->port, dev->speed, dev->state);

    uint16_t max_package = (uint16_t)usb_get_device_max_packet(dev);
    if (!max_package)
        return -EIO;

    max_package = (max_package == 9) ? 512 : max_package;
    debug("[USB_CORE] Device max package %u\n", max_package);

    return 0;
}

int usb_core_init()
{
    for (int i = 0; i < USB_MAX_HOSTS; i++) {
        hosts[i] = NULL;
    }
    for (int i = 0; i < 256; i++) {
        devices[i] = NULL;
    }

    // set signal handler
    signal_action_t irqhand;
    irqhand.handler = _host_signals_handler;
    signal_action(SIGNAL_USER_BASE, &irqhand);
    return 0;
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
                            }
                        }
                    }
                    host->ports[p].state_change = 0;
                }
            }
        }
        event_flags &= ~USB_EVENT_PORT_CHANGE;
    }
}