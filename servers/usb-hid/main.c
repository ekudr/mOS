#include <mosstd.h>
#include <cap.h>
#include <devman.h>
#include <libsys/ipc.h>
#include <ipc.h>
#include <string.h>
#include <sched.h>
#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>
#include <libsys/usb/usb-urb.h>

#define HID_MAX_DEVICES     8

// HID Boot Keyboard report: modifier, reserved, keycodes[6]
#define HID_KBD_REPORT_LEN  8
// HID Boot Mouse report: buttons, dx, dy
#define HID_MOUSE_REPORT_LEN 4

// HID class-specific requests
#define HID_REQ_SET_PROTOCOL   0x0b
#define HID_REQ_SET_IDLE       0x0a
#define HID_PROTOCOL_BOOT      0

typedef struct hid_device {
    uint8_t  slot_id;
    uint8_t  host_id;
    uint8_t  int_ep_id;
    uint8_t  speed;
    uint32_t route_string;
    int      initialized;
    int      disabled;
} hid_device_t;

static hid_device_t devs[HID_MAX_DEVICES];
static int          ndevs;
static int          hid_cap;
static int          usb_core_cap;

// ctrl xfer shared buf (host->buf, provided by usb-core in bind reply)
static cap_id_t     shm_cap;
static void        *shm_buf;

// direct xfer path: class driver-owned shm, passed to xHCI
static cap_id_t     notif_cap;
static cap_id_t     data_shm_cap;
static void        *data_shm;
static cap_id_t     result_shm_cap;
static urb_result_t *result_table;
static cap_id_t     xhci_xfer_cap;

static int hid_ctrl_xfer(hid_device_t *d, usb_control_request_t *req,
                         void *data, uint16_t data_len)
{
    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src          = IPC_SRC_CLASS;
    cmd->cmd_type     = IPC_CLASS_DEV_CTRL_XFER;
    cmd->ctrl.slot_id = d->slot_id;
    cmd->ctrl.host_id = d->host_id;
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

// Submit interrupt-IN URB directly to xHCI and wait for completion.
static int hid_poll_interrupt(hid_device_t *d, void *buf, uint8_t len)
{
    urb_t u;
    u.slot_id = d->slot_id;
    u.ep_id   = d->int_ep_id;
    u.dir     = USB_DIR_IN;
    u.len     = len;
    u.offset  = 0;
    int r = urb_submit(&u);
    if (r < 0) return r;
    r = urb_wait(&u);
    if (r == 0 && buf) memcpy(buf, data_shm, u.actual);
    return r == 0 ? (int)u.actual : r;
}

static void hid_init_device(hid_device_t *d)
{
    usb_control_request_t req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
        .bRequest      = HID_REQ_SET_PROTOCOL,
        .wValue        = HID_PROTOCOL_BOOT,
        .wIndex        = 0,
        .wLength       = 0,
    };
    int ret = hid_ctrl_xfer(d, &req, NULL, 0);
    if (ret < 0)
        debug("[HID] Slot %u SET_PROTOCOL(boot) failed %d\n", d->slot_id, ret);

    usb_control_request_t idle_req = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
        .bRequest      = HID_REQ_SET_IDLE,
        .wValue        = 0,
        .wIndex        = 0,
        .wLength       = 0,
    };
    hid_ctrl_xfer(d, &idle_req, NULL, 0);

    debug("[HID] Bound slot=%u host=%u int_ep=%u route=0x%x (#%d)\n",
          d->slot_id, d->host_id, d->int_ep_id, d->route_string, ndevs);
}

static void hid_decode_keyboard(const uint8_t *r)
{
    if (r[0] == 0 && r[2] == 0 && r[3] == 0 && r[4] == 0 &&
        r[5] == 0 && r[6] == 0 && r[7] == 0)
        return;
    debug("[HID] kbd mod=0x%02x keys=%02x %02x %02x %02x %02x %02x\n",
          r[0], r[2], r[3], r[4], r[5], r[6], r[7]);
}

static void hid_decode_mouse(const uint8_t *r, int len)
{
    if (len < 3) return;
    int8_t dx = (int8_t)r[1];
    int8_t dy = (int8_t)r[2];
    if (r[0] == 0 && dx == 0 && dy == 0) return;
    debug("[HID] mouse btn=0x%02x dx=%d dy=%d\n", r[0], dx, dy);
}

static int hid_poll_bind(void)
{
    if (ndevs >= HID_MAX_DEVICES)
        return 0;

    usb_class_cmd_t *cmd = (usb_class_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->src                = IPC_SRC_CLASS;
    cmd->cmd_type           = IPC_CLASS_POLL_BIND;
    cmd->poll_bind.class_id = USB_CLASS_ID_HID;

    ipc_set_cap(0, notif_cap);
    ipc_set_cap(1, data_shm_cap);
    ipc_set_cap(2, result_shm_cap);
    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 3, 0);
    info = ipc_call(usb_core_cap, info);

    uint8_t slot_id = (uint8_t)ipc_getMR(0);
    if (slot_id == 0)
        return 0;

    hid_device_t *d = &devs[ndevs];
    d->slot_id      = slot_id;
    d->host_id      = (uint8_t)ipc_getMR(1);
    d->int_ep_id    = (uint8_t)ipc_getMR(2);
    d->route_string = (uint32_t)ipc_getMR(3);
    d->speed        = (uint8_t)ipc_getMR(4);
    d->initialized  = 0;
    d->disabled     = 0;

    if (!xhci_xfer_cap) {
        xhci_xfer_cap = (cap_id_t)ipc_getMR(5);
        urb_subsystem_init(xhci_xfer_cap, notif_cap, data_shm, result_table);
    }

    if (!shm_buf) {
        shm_cap = ipc_get_cap(0);
        shm_buf = ipc_shm_attach(shm_cap, NULL, 0);
        if (!shm_buf) {
            debug("[HID] shm_attach failed — bind dropped\n");
            return 0;
        }
    }

    ndevs++;
    return 1;
}

int main(void)
{
    debug("USB HID class driver (boot protocol, async URB)\n");

    hid_cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (hid_cap < 0) {
        debug("[HID] create_capability failed\n");
        return -1;
    }

    if (devman_register("usb-hid", hid_cap) < 0) {
        debug("[HID] devman_register failed\n");
        return -1;
    }

    notif_cap = notif_create(CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT | CRIGHT_NOTIFY);
    if (notif_cap < 0) {
        debug("[HID] notif_create failed\n");
        return -1;
    }
    if (notif_bind(notif_cap) < 0) {
        debug("[HID] notif_bind failed\n");
        return -1;
    }

    data_shm_cap = cap_shmem_create(PAGE_SIZE, CRIGHT_GRANT | CRIGHT_MAP);
    if (data_shm_cap < 0) {
        debug("[HID] cap_shmem_create data failed\n");
        return -1;
    }
    data_shm = ipc_shm_attach(data_shm_cap, NULL, 0);
    if (!data_shm) {
        debug("[HID] data_shm attach failed\n");
        return -1;
    }
    memset(data_shm, 0, PAGE_SIZE);

    result_shm_cap = cap_shmem_create(URB_TABLE_SIZE, CRIGHT_GRANT | CRIGHT_MAP);
    if (result_shm_cap < 0) {
        debug("[HID] cap_shmem_create result failed\n");
        return -1;
    }
    result_table = (urb_result_t *)ipc_shm_attach(result_shm_cap, NULL, 0);
    if (!result_table) {
        debug("[HID] result_table attach failed\n");
        return -1;
    }
    memset(result_table, 0, URB_TABLE_SIZE);

    usb_core_cap = devman_lookup("usb-core");
    while (usb_core_cap <= 0) {
        sched_yield();
        usb_core_cap = devman_lookup("usb-core");
    }
//    debug("[HID] usb-core cap 0x%x\n", usb_core_cap);

    uint8_t report[HID_KBD_REPORT_LEN];

    for (;;) {
        hid_poll_bind();

        for (int i = 0; i < ndevs; i++) {
            if (!devs[i].initialized) {
                hid_init_device(&devs[i]);
                devs[i].initialized = 1;
            }
        }

        for (int i = 0; i < ndevs; i++) {
            if (devs[i].disabled)
                continue;

            int actual = hid_poll_interrupt(&devs[i], report, sizeof(report));
            if (actual < 0) {
                devs[i].disabled = 1;
                debug("[HID] Slot %u int-IN error %d — disabled\n",
                      devs[i].slot_id, actual);
                continue;
            }
            if (actual == 0)
                continue;

            if (actual >= HID_KBD_REPORT_LEN)
                hid_decode_keyboard(report);
            else
                hid_decode_mouse(report, actual);
        }
    }

    return 0;
}
