#include <mosstd.h>
#include <cap.h>
#include <vfs.h>
#include <devman.h>
#include <libsys/ipc.h>
#include <ipc.h>
#include <string.h>
#include <sched.h>
#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>

#define HUB_MAX_PORTS   7
#define HUB_RESET_TRIES 50
#define MAX_HUBS        4

typedef struct hub_instance {
    uint8_t  slot_id;
    uint8_t  host_id;
    uint8_t  int_ep_id;
    uint8_t  num_ports;
    uint8_t  speed;
    uint32_t route_string;
    int      initialized;
    uint16_t int_fail_count;
    uint8_t  int_disabled;
} hub_instance_t;

static hub_instance_t hubs[MAX_HUBS];
static int            nhubs;
static int            hub_cap;
static int            usb_core_cap;
static cap_id_t       shm_cap;
static void          *shm_buf;

/* Compute child device route string by inserting hub_port into first zero nibble */
static uint32_t child_route(hub_instance_t *h, uint8_t port)
{
    uint32_t r = h->route_string;
    for (int shift = 0; shift < 20; shift += 4) {
        if (((r >> shift) & 0xf) == 0)
            return r | ((uint32_t)port << shift);
    }
    return r;
}

/* Send a control transfer request to usb-core for the hub device */
static int hub_ctrl_xfer(hub_instance_t *h, usb_control_request_t *req, void *data, uint16_t data_len)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src          = IPC_SRC_CLASS;
    cmd->cmd_type     = IPC_CLASS_DEV_CTRL_XFER;
    cmd->ctrl.slot_id = h->slot_id;
    cmd->ctrl.host_id = h->host_id;
    memcpy(cmd->ctrl.setup_pkt, req, 8);
    cmd->ctrl.data_len = data_len;

    /* OUT with data: copy to shared buf before the call */
    if (data_len > 0 && !(req->bmRequestType & USB_DIR_IN) && data)
        memcpy(shm_buf, data, data_len);

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(usb_core_cap, info);

    int ret = (int)ipc_getMR(0);
    /* IN: result is in shm_buf (same memory as host->buf) after the call */
    if (ret == 0 && data_len > 0 && (req->bmRequestType & USB_DIR_IN) && data)
        memcpy(data, shm_buf, data_len);
    return ret;
}

static int hub_get_hub_descriptor(hub_instance_t *h, usb_hub_descriptor_t *desc)
{
    if (h->speed == USB_SPEED_SUPER) {
        usb_ss_hub_descriptor_t ss;
        usb_control_request_t req = {
            .bmRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
            .bRequest      = USB_REQ_GET_DESCRIPTOR,
            .wValue        = (USB_DT_SS_HUB << 8),
            .wIndex        = 0,
            .wLength       = sizeof(ss),
        };
        int ret = hub_ctrl_xfer(h, &req, &ss, sizeof(ss));
        if (ret == 0) {
            desc->bLength             = ss.bLength;
            desc->bDescriptorType     = USB_DT_HUB;
            desc->bNbrPorts           = ss.bNbrPorts;
            desc->wHubCharacteristics = ss.wHubCharacteristics;
            desc->bPwrOn2PwrGood      = ss.bPwrOn2PwrGood;
            desc->bHubContrCurrent    = ss.bHubContrCurrent;
        }
        return ret;
    }
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
        .bRequest      = USB_REQ_GET_DESCRIPTOR,
        .wValue        = (USB_DT_HUB << 8),
        .wIndex        = 0,
        .wLength       = sizeof(*desc),
    };
    return hub_ctrl_xfer(h, &req, desc, sizeof(*desc));
}

static int hub_get_port_status(hub_instance_t *h, uint8_t port, usb_port_status_t *st)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_OTHER,
        .bRequest      = USB_REQ_GET_STATUS,
        .wValue        = 0,
        .wIndex        = port,
        .wLength       = sizeof(*st),
    };
    return hub_ctrl_xfer(h, &req, st, sizeof(*st));
}

static int hub_set_port_feature(hub_instance_t *h, uint8_t port, uint16_t feat)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER,
        .bRequest      = USB_REQ_SET_FEATURE,
        .wValue        = feat,
        .wIndex        = port,
        .wLength       = 0,
    };
    return hub_ctrl_xfer(h, &req, NULL, 0);
}

static int hub_clear_port_feature(hub_instance_t *h, uint8_t port, uint16_t feat)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER,
        .bRequest      = USB_REQ_CLEAR_FEATURE,
        .wValue        = feat,
        .wIndex        = port,
        .wLength       = 0,
    };
    return hub_ctrl_xfer(h, &req, NULL, 0);
}

/* Report a newly connected child device to usb-core */
static void hub_report_connect(hub_instance_t *h, uint8_t port, uint8_t speed)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src                       = IPC_SRC_CLASS;
    cmd->cmd_type                  = IPC_CLASS_PORT_CONNECT;
    cmd->port_connect.slot_id      = h->slot_id;
    cmd->port_connect.hub_port     = port;
    cmd->port_connect.speed        = speed;
    cmd->port_connect.route_string = child_route(h, port);

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(usb_core_cap, info);
    debug("[HUB] Slot %u port %u connect ret=%d\n", h->slot_id, port, (int)ipc_getMR(0));
}

/* Report a disconnected child device to usb-core */
static void hub_report_disconnect(hub_instance_t *h, uint8_t port)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src                          = IPC_SRC_CLASS;
    cmd->cmd_type                     = IPC_CLASS_PORT_DISCONNECT;
    cmd->port_disconnect.slot_id      = h->slot_id;
    cmd->port_disconnect.hub_port     = port;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    ipc_call(usb_core_cap, info);
}

/* Speed from port status bits; SS hub → children always SS, HS hub → use port bits */
static uint8_t port_speed(hub_instance_t *h, uint16_t wPortStatus)
{
    if (h->speed == USB_SPEED_SUPER)
        return USB_SPEED_SUPER;
    if (wPortStatus & USB_PORT_STAT_HIGH_SPEED)
        return USB_SPEED_HIGH;
    if (wPortStatus & USB_PORT_STAT_LOW_SPEED)
        return USB_SPEED_LOW;
    return USB_SPEED_FULL;
}

/* Handle a port status change event */
static void hub_handle_port_change(hub_instance_t *h, uint8_t p)
{
    usb_port_status_t st;
    if (hub_get_port_status(h, p, &st) < 0) {
        debug("[HUB] GetPortStatus failed slot %u port %u\n", h->slot_id, p);
        return;
    }
    debug("[HUB] Slot %u port %u status=0x%04x change=0x%04x\n",
          h->slot_id, p, st.wPortStatus, st.wPortChange);

    if (st.wPortChange & USB_PORT_CHANGE_CONNECTION) {
        hub_clear_port_feature(h, p, USB_PORT_FEAT_C_CONNECTION);

        if (st.wPortStatus & USB_PORT_STAT_CONNECTION) {
            /* Device attached — issue port reset */
            hub_set_port_feature(h, p, USB_PORT_FEAT_RESET);

            /* Poll for reset completion */
            for (int i = 0; i < HUB_RESET_TRIES; i++) {
                sched_yield();
                hub_get_port_status(h, p, &st);
                if (st.wPortChange & USB_PORT_CHANGE_RESET)
                    break;
            }
            hub_clear_port_feature(h, p, USB_PORT_FEAT_C_RESET);

            if (st.wPortStatus & USB_PORT_STAT_ENABLE)
                hub_report_connect(h, p, port_speed(h, st.wPortStatus));
            else
                debug("[HUB] Slot %u port %u not enabled after reset\n", h->slot_id, p);
        } else {
            /* Device detached */
            hub_report_disconnect(h, p);
        }
    }

    if (st.wPortChange & USB_PORT_CHANGE_BH_RESET)
        hub_clear_port_feature(h, p, USB_PORT_FEAT_C_BH_PORT_RESET);

    if (st.wPortChange & USB_PORT_CHANGE_LINK_STATE)
        hub_clear_port_feature(h, p, USB_PORT_FEAT_C_PORT_LINK_STATE);
}

/* Submit interrupt-IN transfer; returns actual bytes received or <0 on error */
static int hub_poll_interrupt(hub_instance_t *h, void *buf, uint8_t len)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src          = IPC_SRC_CLASS;
    cmd->cmd_type     = IPC_CLASS_DEV_XFER_SUBMIT;
    cmd->xfer.slot_id = h->slot_id;
    cmd->xfer.host_id = h->host_id;
    cmd->xfer.ep_id   = h->int_ep_id;
    cmd->xfer.dir     = USB_DIR_IN;
    cmd->xfer.len     = len;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(usb_core_cap, info);

    int ret = (int)ipc_getMR(0);
    if (ret == 0 && buf)
        memcpy(buf, shm_buf, len);
    return ret < 0 ? ret : (int)ipc_getMR(1);
}

/* Initialise one hub instance: get descriptor, power all ports */
static void hub_init_instance(hub_instance_t *h)
{
    usb_hub_descriptor_t desc;
    if (hub_get_hub_descriptor(h, &desc) < 0) {
        debug("[HUB] Slot %u: failed to read descriptor — assuming 4 ports\n", h->slot_id);
        h->num_ports = 4;
    } else {
        h->num_ports = desc.bNbrPorts;
        if (h->num_ports > HUB_MAX_PORTS)
            h->num_ports = HUB_MAX_PORTS;
        debug("[HUB] Slot %u: %u ports, pwrOn2PwrGood=%ums\n",
              h->slot_id, h->num_ports, (unsigned)desc.bPwrOn2PwrGood * 2);
    }

    for (uint8_t p = 1; p <= h->num_ports; p++)
        hub_set_port_feature(h, p, USB_PORT_FEAT_POWER);
}

/* Pull next pending hub-bind from usb-core. Returns 1 on success, 0 if none. */
static int hub_poll_bind(void)
{
    if (nhubs >= MAX_HUBS)
        return 0;

    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src                 = IPC_SRC_CLASS;
    cmd->cmd_type            = IPC_CLASS_POLL_BIND;
    cmd->poll_bind.class_id  = USB_CLASS_ID_HUB;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(usb_core_cap, info);

    uint8_t slot_id = (uint8_t)ipc_getMR(0);
    if (slot_id == 0)
        return 0;

    hub_instance_t *h = &hubs[nhubs];
    h->slot_id        = slot_id;
    h->host_id        = (uint8_t)ipc_getMR(1);
    h->int_ep_id      = (uint8_t)ipc_getMR(2);
    h->route_string   = (uint32_t)ipc_getMR(3);
    h->speed          = (uint8_t)ipc_getMR(4);
    h->initialized    = 0;
    h->int_fail_count = 0;
    h->int_disabled   = 0;

    if (!shm_buf) {
        shm_cap = ipc_get_cap(0);
        shm_buf = ipc_shm_attach(shm_cap, NULL, 0);
        if (!shm_buf) {
            debug("[HUB] shm_attach failed — bind dropped\n");
            return 0;
        }
    }

    nhubs++;
    debug("[HUB] Bound slot=%u host=%u int_ep=%u route=0x%x (#%d)\n",
          h->slot_id, h->host_id, h->int_ep_id, h->route_string, nhubs);
    return 1;
}

int main(void)
{
    debug("USB Hub class driver ver 0.0.2 (multi-hub, pull-bind)\n");

    /* hub_cap is created and registered only because devman_register needs a cap;
     * with pull-based bind, no one calls into us, so we never receive on it. */
    hub_cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (hub_cap < 0) {
        debug("[HUB] create_capability failed\n");
        return -1;
    }

    if (devman_register("usb-hub", hub_cap) < 0) {
        debug("[HUB] devman_register failed\n");
        return -1;
    }

    usb_core_cap = devman_lookup("usb-core");
    while (usb_core_cap <= 0) {
        sched_yield();
        usb_core_cap = devman_lookup("usb-core");
    }
    debug("[HUB] usb-core cap 0x%x\n", usb_core_cap);

    for (;;) {
        /* (a) Pull one pending bind per iteration */
        hub_poll_bind();

        /* (b) Init any newly bound hubs */
        for (int i = 0; i < nhubs; i++) {
            if (!hubs[i].initialized) {
                hub_init_instance(&hubs[i]);
                hubs[i].initialized = 1;
            }
        }

        /* (c) Round-robin interrupt-IN poll across all active hubs */
#define HUB_INT_FAIL_MAX 8
        for (int i = 0; i < nhubs; i++) {
            if (hubs[i].int_disabled)
                continue;
            uint8_t bitmap = 0;
            int actual = hub_poll_interrupt(&hubs[i], &bitmap, 1);
            if (actual == -ETIMEOUT) {
                // No data ready; normal for idle hub. Don't count as failure
                // — otherwise idle hubs disable themselves after a few rounds.
                continue;
            }
            if (actual <= 0) {
                hubs[i].int_fail_count++;
                if (hubs[i].int_fail_count >= HUB_INT_FAIL_MAX) {
                    hubs[i].int_disabled = 1;
                    debug("[HUB] Slot %u interrupt-IN disabled after %u failures\n",
                          hubs[i].slot_id, hubs[i].int_fail_count);
                }
                continue;
            }
            hubs[i].int_fail_count = 0;
            debug("[HUB] Slot %u status bitmap 0x%02x\n", hubs[i].slot_id, bitmap);
            /* bit 0 = hub status change (ignore for now); bits 1..N = port N change */
            for (uint8_t p = 1; p <= hubs[i].num_ports; p++) {
                if (bitmap & (1u << p))
                    hub_handle_port_change(&hubs[i], p);
            }
        }

        sched_yield();
    }

    return 0;
}
