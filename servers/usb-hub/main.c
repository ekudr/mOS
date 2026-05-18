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
#include <libsys/usb/usb-urb.h>

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
    int      disabled;
} hub_instance_t;

static hub_instance_t hubs[MAX_HUBS];
static int            nhubs;
static int            hub_cap;
static int            usb_core_cap;

// ctrl xfer shared buf (host->buf, provided by usb-core in bind reply)
static cap_id_t       shm_cap;
static void          *shm_buf;

// direct xfer path: class driver-owned shm, passed to xHCI
static cap_id_t       notif_cap;
static cap_id_t       data_shm_cap;
static void          *data_shm;
static cap_id_t       result_shm_cap;
static urb_result_t  *result_table;
static cap_id_t       xhci_xfer_cap;

static uint32_t child_route(hub_instance_t *h, uint8_t port)
{
    uint32_t r = h->route_string;
    for (int shift = 0; shift < 20; shift += 4) {
        if (((r >> shift) & 0xf) == 0)
            return r | ((uint32_t)port << shift);
    }
    return r;
}

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

    if (data_len > 0 && !(req->bmRequestType & USB_DIR_IN) && data)
        memcpy(shm_buf, data, data_len);

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(usb_core_cap, info);

    int ret = (int)ipc_getMR(0);
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
            hub_set_port_feature(h, p, USB_PORT_FEAT_RESET);

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
            hub_report_disconnect(h, p);
        }
    }

    if (st.wPortChange & USB_PORT_CHANGE_BH_RESET)
        hub_clear_port_feature(h, p, USB_PORT_FEAT_C_BH_PORT_RESET);

    if (st.wPortChange & USB_PORT_CHANGE_LINK_STATE)
        hub_clear_port_feature(h, p, USB_PORT_FEAT_C_PORT_LINK_STATE);
}

// Submit interrupt-IN URB directly to xHCI and wait for completion.
static int hub_poll_interrupt(hub_instance_t *h, void *buf, uint8_t len)
{
    urb_t u;
    u.slot_id = h->slot_id;
    u.ep_id   = h->int_ep_id;
    u.dir     = USB_DIR_IN;
    u.len     = len;
    u.offset  = 0;
    int r = urb_submit(&u);
    if (r < 0) return r;
    r = urb_wait(&u);
 //       debug("__________-URB-SUBMIT- actual %d status %d ret %d - ____________\n", u.actual, u.status, r);
    if (r == 0 && buf) memcpy(buf, data_shm, u.actual);
    return r == 0 ? (int)u.actual : r;
}

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

static int hub_poll_bind(void)
{
    if (nhubs >= MAX_HUBS)
        return 0;

    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src                 = IPC_SRC_CLASS;
    cmd->cmd_type            = IPC_CLASS_POLL_BIND;
    cmd->poll_bind.class_id  = USB_CLASS_ID_HUB;

    ipc_set_cap(0, notif_cap);
    ipc_set_cap(1, data_shm_cap);
    ipc_set_cap(2, result_shm_cap);
    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 3, 0);
    info = ipc_call(usb_core_cap, info);

    uint8_t slot_id = (uint8_t)ipc_getMR(0);
    if (slot_id == 0)
        return 0;

    hub_instance_t *h = &hubs[nhubs];
    h->slot_id      = slot_id;
    h->host_id      = (uint8_t)ipc_getMR(1);
    h->int_ep_id    = (uint8_t)ipc_getMR(2);
    h->route_string = (uint32_t)ipc_getMR(3);
    h->speed        = (uint8_t)ipc_getMR(4);
    h->initialized  = 0;
    h->disabled     = 0;

    // Save xhci_xfer_cap and init URB subsystem on first bind
    if (!xhci_xfer_cap) {
        xhci_xfer_cap = (cap_id_t)ipc_getMR(5);
        urb_subsystem_init(xhci_xfer_cap, notif_cap, data_shm, result_table);
    }

    // Attach ctrl-xfer shared buf on first bind
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
    debug("USB Hub class driver ver 0.0.3 (async URB)\n");

    hub_cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (hub_cap < 0) {
        debug("[HUB] create_capability failed\n");
        return -1;
    }

    if (devman_register("usb-hub", hub_cap) < 0) {
        debug("[HUB] devman_register failed\n");
        return -1;
    }

    notif_cap = notif_create(CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT | CRIGHT_NOTIFY);
    if (notif_cap < 0) {
        debug("[HUB] notif_create failed\n");
        return -1;
    }
    if (notif_bind(notif_cap) < 0) {
        debug("[HUB] notif_bind failed\n");
        return -1;
    }

    data_shm_cap = cap_shmem_create(PAGE_SIZE, CRIGHT_GRANT | CRIGHT_MAP);
    if (data_shm_cap < 0) {
        debug("[HUB] cap_shmem_create data failed\n");
        return -1;
    }
    data_shm = ipc_shm_attach(data_shm_cap, NULL, 0);
    if (!data_shm) {
        debug("[HUB] data_shm attach failed\n");
        return -1;
    }
    memset(data_shm, 0, PAGE_SIZE);

    result_shm_cap = cap_shmem_create(URB_TABLE_SIZE, CRIGHT_GRANT | CRIGHT_MAP);
    if (result_shm_cap < 0) {
        debug("[HUB] cap_shmem_create result failed\n");
        return -1;
    }
    result_table = (urb_result_t *)ipc_shm_attach(result_shm_cap, NULL, 0);
    if (!result_table) {
        debug("[HUB] result_table attach failed\n");
        return -1;
    }
    memset(result_table, 0, URB_TABLE_SIZE);

    usb_core_cap = devman_lookup("usb-core");
    while (usb_core_cap <= 0) {
        sched_yield();
        usb_core_cap = devman_lookup("usb-core");
    }
//    debug("[HUB] usb-core cap 0x%x\n", usb_core_cap);

    for (;;) {
        hub_poll_bind();

        for (int i = 0; i < nhubs; i++) {
            if (!hubs[i].initialized) {
                hub_init_instance(&hubs[i]);
                hubs[i].initialized = 1;
            }
        }

        for (int i = 0; i < nhubs; i++) {
            if (hubs[i].disabled)
                continue;
            uint8_t bitmap = 0;
            int actual = hub_poll_interrupt(&hubs[i], &bitmap, 1);
            if (actual < 0) {
                hubs[i].disabled = 1;
                debug("[HUB] Slot %u int-IN error %d — disabled\n",
                      hubs[i].slot_id, actual);
                continue;
            }
            if (actual == 0)
                continue;
            debug("[HUB] Slot %u status bitmap 0x%02x\n", hubs[i].slot_id, bitmap);
            for (uint8_t p = 1; p <= hubs[i].num_ports; p++) {
                if (bitmap & (1u << p))
                    hub_handle_port_change(&hubs[i], p);
            }
        }

        sched_yield();
    }

    return 0;
}
