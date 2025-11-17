#include <mosstd.h>
#include <libsys/cap.h>
#include <libsys/ipc.h>
#include <block_dev.h>

#include <string.h>
#include <devman.h>
#include <vfs.h>

#include "mmc.h"


int open_dev(const char *name, int buf_cap, uint32_t buf_size);
int close_dev(int desc);
int read_dev(int desc, uint64_t start, uint64_t blocks);


cap_id_t cap = 0;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}


void init_server()
{
    char *n;
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    debug("SD/MMC driver created cap %d\n", cap);
 
    int ret = devman_register("sdmmc0", cap);    
//    debug("[TTY] Register status %d\n", ret);
    if (ret < 0) {
        panic("devman register");
    }

    int vfs = vfs_open("/dev/sdmmc0");
//    debug("[TTY] open vfs returned %d\n", vfs);
    if (vfs < 0) {
        vfs = vfs_create("/dev/sdmmc0", VFS_DEVICE, cap);
        n = malloc(32);
        memset(n, 0, 32);
        snprintf_(n, 31, "sdmmc%d", 0);
        set_device_name(0, n);        
    }
    debug("[SDMMC] create vfs returned %d\n", vfs);
    char name[32];
   
    for (int i=0; i<128; i++){
        if (get_device_start(i+1)) {
            snprintf_(name, 32, "/dev/sdmmc0p%d", i);
            if (vfs_create(name, VFS_DEVICE, cap)) {
                n = malloc(32);
                memset(n, 0, 32);
                snprintf_(n, 31, "sdmmc0p%d", i);
                set_device_name(i+1, n);
            }
            
        }
    }

}

void init_mmc()
{
    int err;
    mmc_init_host();
    err = mmc_init();
    debug("[MMC] mmc_init returned %d\n", err);
    if (err<0){
        panic("[SDMMC] MMC Init err");
    }
    
    init_dev();
    if (err<0){
        panic("[SDMMC] Device Init err");
    }
}


void main()
{    
    debug("MMC Driver v.0.0.1\n");
    
    init_mmc();
    init_server();

    /* main IPC loop */
    while (1) {
        
        struct sdmmc_msg *sd_msg = (struct sdmmc_msg *)get_ipc_buffer()->msg;

        uint64_t info = ipc_recv(cap, NULL);
        int ret = (int)label_from_msginfo_word(info);
        if (ret < 0 || !length_from_msginfo_word(info)) {
            debug("[MMC] Error receiving message %d info 0x%lX\n", ret, info);
        }
        
        switch (sd_msg->type) {
        case MMC_OP_OPEN: {
//               sd_msg->msg.open.buf_cap = ipc_get_cap(0);
                int desc = open_dev(sd_msg->msg.open.name, ipc_get_cap(0),
                                                sd_msg->msg.open.buf_size);

                memset(sd_msg, 0, sizeof(*sd_msg));
                sd_msg->type = MMC_OP_REPLY;
                sd_msg->msg.open.desc = desc;
                sd_msg->msg.open.total_blocks = get_device_blocks_by_desc(desc);
                info = msginfo_word_new(0,sizeof(*sd_msg)/8, 0, 0);
                ipc_reply(info);
                break;
            }
        case MMC_OP_CLOSE: {
                int ret = close_dev(sd_msg->msg.ctrl.desc);

                memset(sd_msg, 0, sizeof(*sd_msg));
                sd_msg->type = MMC_OP_REPLY;
                sd_msg->msg.ctrl.desc = ret;

                info = msginfo_word_new(0,sizeof(*sd_msg)/8, 0, 0);
                ipc_reply(info);
                break;
            }
        // case MMC_OP_GET_INFO: {
        //     struct mmc_info info;
        //     info.block_size = MMC_BLOCK_SIZE;
        //     info.capacity_blocks = 0; /* optional: implement CMD9/CMD8 to read CSD and compute*/
        //     info.flags = 0;
        //     outmsg.type = inmsg.type;
        //     /* copy into message union — ensure your message union is large enough */
        //     memcpy(&outmsg.u, &info, sizeof(info));
        //     ipc_reply(reply_cap, &outmsg, sizeof(outmsg));
        //     break;
        // }
        case MMC_OP_READ_BLOCK: {
                uint64_t blocks = read_dev(sd_msg->msg.read.desc, sd_msg->msg.read.start, sd_msg->msg.read.blocks);
                memset(sd_msg, 0, sizeof(*sd_msg));
                sd_msg->type = MMC_OP_REPLY;
                sd_msg->msg.read.blocks = blocks;
                info = msginfo_word_new(0,sizeof(*sd_msg)/8, 0, 0);
                ipc_reply(info);            
                break;
            }
        // case MMC_OP_WRITE_BLOCK: {
        //     uint32_t block = (uint32_t)inmsg.mr[0];
        //     void *buf = (void *)(uintptr_t)inmsg.mr[1];
        //     if (!buf) {
        //         outmsg.mr[0] = (uintptr_t)-1;
        //         ipc_reply(reply_cap, &outmsg);

        //         break;
        //     }
        //     int rc = sdhci_write_block_pio(base, block, buf);
        //     outmsg.mr[0] = (uintptr_t)rc;
        //     ipc_reply(reply_cap, &outmsg, sizeof(outmsg));
        //     break;
        //     }

        default:
            memset(sd_msg, 0, sizeof(*sd_msg));
            sd_msg->type = MMC_OP_INVALID;
            info = msginfo_word_new(0,sizeof(*sd_msg)/8, 0, 0);
            ipc_reply(info);
            break;
        }
    } /* main loop */
    /* never reached, but cleanup if ever needed */
//    maybe_unsubscribe_irq();
    
}
