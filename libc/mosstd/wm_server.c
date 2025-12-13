#include <stdint.h>
#include <mosstd.h>
#include <sched.h>
#include <string.h>
#include <wm_msg.h>
#include <libsys/ipc.h>
#include <nameserver.h>

int wm_cap = 0;

static int get_wm_cap()
{
    if (!wm_cap) {
        while(1) {
            wm_cap = ns_lookup_cap("wm"); 
            if (wm_cap > 0) break; 
            sched_yield();
        }        
    } 

    return wm_cap;
}

int wm_create_window(uint32_t width, uint32_t height, uint32_t flags, uint32_t *wid, int *shm_cap)
{
    int cap = get_wm_cap();

    wm_msg_hdr_t *hdr = (wm_msg_hdr_t *)&get_ipc_buffer()->msg[0];
    wm_msg_create_t *msg = (wm_msg_create_t *)&get_ipc_buffer()->msg[1];

    hdr->type = WM_CMD_CREATE_WINDOW;
    hdr->length = sizeof(*msg);

    msg->width  = width;
    msg->height = height;
    msg->flags  = flags;

    uint64_t info = msginfo_word_new(0, 3, 0, 0);
    info = ipc_call(cap, info);
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("[WMLIB] Error creating window ipc %d info 0x%lX\n", ret, info);
        return ret;
    }
    uint64_t st = ipc_getMR(0);
    if (st != WM_EVT_WINDOW_CREATED) {
        return -st;
    }
//debug("[WMLIB] Creating win ret 0x%lX info 0x%lX MR1 0x%lX shm 0x%lX\n", st, info, ipc_getMR(1), ipc_get_cap(0));
    *wid = (uint32_t) ipc_getMR(1);
    *shm_cap = ipc_get_cap(0);

    return SUCCESS;
}
