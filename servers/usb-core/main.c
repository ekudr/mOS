#include <mosstd.h>
#include <cap.h>
#include <vfs.h>
#include <devman.h>
#include <sched.h>

#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>
#include "usb-core.h"

int cap;
int xhci_cap;
pid_t pid;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

static int init_server(void)
{
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    debug("USB core driver created cap 0x%lX\n", cap);
 
    int ret = devman_register("usb-core", cap);

    if (ret < 0) {
        return ret;
    }

    pid = getpid();

    return SUCCESS;
}

int main()
{
    debug("USB Core driver ver 0.0.3\n");

    int ret;

    ret = init_server();
    if (ret < 0) panic("xHCI server init");

    usb_core_init();

    for(;;) {
        uint64_t sender;

        handle_usb_events();
        
        uint64_t info = ipc_nb_recv(cap, &sender);
        if(!sender) {
            continue;
        }

        switch (ipc_getMR(0)){
            case IPC_SRC_HOST: {
                handle_host_ipc(sender, info);
                break;
            }

            case IPC_SRC_CLASS: {
                handle_class_ipc(sender, info);
                break;
            }

            default:
                break;
        }
    }

    return 0;
}