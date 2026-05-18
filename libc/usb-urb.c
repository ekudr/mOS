#include <mosstd.h>
#include <libsys/usb/usb-urb.h>
#include <libsys/usb/usb-ipc.h>
#include <libsys/ipc.h>
#include <string.h>

static cap_id_t     _xhci_xfer_cap;
static cap_id_t     _notif_cap;
static void        *_data_shm;
static urb_result_t *_result_table;
static urb_t        *_inflight;

void urb_subsystem_init(cap_id_t xhci_xfer_cap, cap_id_t notif_cap,
                        void *data_shm, urb_result_t *result_table)
{
    _xhci_xfer_cap = xhci_xfer_cap;
    _notif_cap     = notif_cap;
    _data_shm      = data_shm;
    _result_table  = result_table;
    _inflight      = NULL;
}

static void urb_scan_results(void)
{
    urb_t **prev = &_inflight;
    urb_t  *u    = _inflight;
    while (u) {
        urb_result_t *r = &_result_table[URB_RES_IDX(u->slot_id, u->ep_id)];
        if (__atomic_load_n(&r->valid, __ATOMIC_ACQUIRE)) {
            u->status = r->status;
            u->actual = r->actual;
            __atomic_store_n(&r->valid, 0, __ATOMIC_RELEASE);
            __atomic_store_n(&u->done,  1, __ATOMIC_RELEASE);
            *prev = u->next;
            u = u->next;
        } else {
            prev = &u->next;
            u = u->next;
        }
    }
}

int urb_submit(urb_t *u)
{
    u->done   = 0;
    u->status = 0;
    u->actual = 0;
    u->next   = _inflight;
    _inflight = u;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd_type             = IPC_XFER_DIRECT_SUBMIT;
    cmd->direct_xfer.slot_id  = u->slot_id;
    cmd->direct_xfer.ep_id    = u->ep_id;
    cmd->direct_xfer.dir      = u->dir;
    cmd->direct_xfer.len      = u->len;
    cmd->direct_xfer.offset   = u->offset;

    msg_info_t info = msginfo_word_new(0, sizeof(*cmd) / 8, 0, 0);
    info = ipc_call(_xhci_xfer_cap, info);

    return (int)ipc_getMR(0);
}

int urb_wait(urb_t *u)
{
    urb_scan_results();
    while (!__atomic_load_n(&u->done, __ATOMIC_ACQUIRE)) {
        int ret = notif_wait(_notif_cap);
        urb_scan_results();
    }
    return u->status;
}
