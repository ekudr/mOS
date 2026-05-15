#include <mosstd.h>
#include <libsys/cap.h>
#include <vfs.h>
#include <devman.h>
#include <sched.h>
#include <libsys/ipc.h>
#include <ipc.h>
#include <libsys/usb/usb-ipc.h>

#include "main.h"
#include "xhci.h"

int usb3_init();

int hub_enable();
void xhci_hub_events(void);

int cap;
int usb_core_cap;
int buf_cap;
void *buf;
size_t buf_size;
int usb_host_id;
pid_t usb_core_pid;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}


static int init_server(void)
{
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    debug("XHCI driver created cap 0x%lX\n", cap);
 
    int ret = devman_register("xhci0", cap);    
//    debug("[TTY] Register status %d\n", ret);
    if (ret < 0) {
        return ret;
    }

    int vfs = vfs_open("/dev/xhci0");
    debug("[XHCI] open vfs returned %d\n", vfs);
    if (vfs < 0) {
        // ??? transfer cap top vfs
        vfs = vfs_create("/dev/xhci0", VFS_DEVICE, cap);
    }
    debug("[XHCI] create vfs returned %d\n", vfs);
    return SUCCESS;
}

int host_register()
{
    usb_core_cap = devman_lookup("usb-core");
    while (usb_core_cap < 0)
    {
        sched_yield();
        usb_core_cap = devman_lookup("usb-core");
    }
    debug("[XHCI] Found usb-core cap 0x%lX\n", usb_core_cap);

    buf_size = PAGE_SIZE;
    buf_cap = cap_shmem_create(buf_size,  CRIGHT_GRANT | CRIGHT_MAP);
    if (buf_cap < 0) return buf_cap;

    buf = ipc_shm_attach(buf_cap, NULL, 0);
    if (!buf) return -ENOMEM;

    // Send IPC to usb-core to register this host controller
    ipc_setMR(0, IPC_SRC_HOST);
    ipc_setMR(1, IPC_HOST_REGISTER);
    ipc_setMR(2, PAGE_SIZE);
    ipc_setMR(3, _get_max_ports());
    ipc_set_cap(0, cap);
    ipc_set_cap(1, buf_cap);
    msg_info_t info = msginfo_word_new(0, 4, 2, 0);

    info = ipc_call(usb_core_cap, info);
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("[XHCI] Error registring host %d info 0x%lX\n", ret, info);
        return ret;
    }
 
    usb_core_pid = (pid_t)ipc_getMR(1);

    return ipc_getMR(0);
}

int main()
{
    for (size_t i = 0; i < 1000; i++)
    {
        sched_yield();
    }
    
    debug("USB3 SPACEMIT K1 driver ver 0.0.0\n");

    int ret = usb3_init();
    if (ret < 0) panic("xHCI init");

    ret = init_server();
    if (ret < 0) panic("xHCI server init");

    usb_host_id = host_register();
    if (usb_host_id < 0) panic("xHCI host register");
    debug("[XHCI] registered with usb-core, id 0x%lX\n", usb_host_id);

    hub_enable();

    for(;;) {
        xhci_hub_events();

        uint64_t sender;
        uint64_t info = ipc_nb_recv(cap, &sender);
        if (!sender) {
            continue;
        }

//        debug("[XHCI] IPC received from sender 0x%lX info 0x%lX cmd %d\n", sender, info, ipc_getMR(0));
        switch (ipc_getMR(0))
        {
        case IPC_HOST_CTRL_XFER:
            // debug("[XHCI] IPC HOST CTRL XFER\n");
            handle_ctrl_transfer_request(sender, info);
            break;
        case IPC_HOST_NEW_DEVICE:
            handle_new_device(sender, info);
            break;
        case IPC_HOST_ENABLE_SLOT:
            // debug("[XHCI] IPC HOST ENABLE SLOT\n");
            handle_enable_slot(sender, info);
            break;
        case IPC_HOST_DISABLE_SLOT:
            // debug("[XHCI] IPC HOST DISABLE SLOT\n");
            // handle_disable_slot(sender, info);
            break;
        case IPC_HOST_ADDRESS_DEVICE:
            // debug("[XHCI] IPC HOST ADDRESS DEVICE\n");
            // handle_address_device(sender, info);
            break;
        case IPC_HOST_CONFIG_EP:
            // debug("[XHCI] IPC HOST CONFIG EP\n");
            handle_config_ep(sender, info);
            break;
        
        default:
            break;
        } 
    }  
}