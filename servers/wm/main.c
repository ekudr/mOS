#include <stdint.h>
#include <mosstd.h>
#include <libsys/cap.h>
#include <libsys/ipc.h>

#include "wm.h"


int cap;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

void init_server(void)
{
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    debug("TTY driver created cap 0x%lX\n", cap);

    int ret = ns_register_cap(cap, "wm", CRIGHT_SND | CRIGHT_GRANT);
    if (ret < 0) {
        panic("ns reg");
    }   
}

int handle_client_msg(uint64_t sender)
{
    wm_msg_hdr_t *hdr = (wm_msg_hdr_t *)&get_ipc_buffer()->msg[0];
    switch (hdr->type)
    {
        case WM_CMD_CREATE_WINDOW: {
            uint64_t info;
            debug("[WM] received msg type %d lenght 0x%X sender 0x%p\n", hdr->type, hdr->length, sender);            
            if (hdr->length != sizeof(wm_msg_create_t)) {
                // should be handle errror
                ipc_setMR(0, WM_EVT_INVAL_INPUT);
                info = msginfo_word_new(0, 1, 0, 0);
                ipc_reply(info);
                break;
            }

            wm_msg_create_t *msg = (wm_msg_create_t *)&get_ipc_buffer()->msg[1];

            uint32_t wid; int shm;
            int ret = wm_create_window(sender, msg, &wid, &shm);
                        debug("[WM] window created %d shm 0x%X\n", wid, shm); 
            if (ret < 0) {
                ipc_setMR(0, WM_EVT_INVAL_INPUT);
                info = msginfo_word_new(0, 1, 0, 0);
            } else {
                ipc_setMR(0, WM_EVT_WINDOW_CREATED);
                ipc_setMR(1, wid);
                ipc_set_cap(0, shm);
                info = msginfo_word_new(0, 2, 1, 0);                
            }
            ipc_reply(info);
            break;
        }
        
        default:
            break;
    }
}

int main()
{
    debug("Window Manager ver. 0.0.1\n");

    wm_init();
    init_server();

    while (1)
    {
        uint64_t info, sender;
        info = ipc_nb_recv(cap, &sender);
        int ret = (int)label_from_msginfo_word(info);
        if( ret < 0) debug("[WM] received info error %d\n", ret);

        if (sender) handle_client_msg(sender);
        wm_composite();
    }
    
}