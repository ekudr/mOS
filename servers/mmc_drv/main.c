#include <mosstd.h>
#include <cap.h>
#include <ipc.h>


#include <string.h>
#include <devman.h>
#include <vfs.h>

#include "mmc.h"
#include "mmc_ipc.h"

int open_dev(const char *name, int buf_cap, uint32_t buf_size);
int read_dev(int desc, uint64_t start, uint64_t blocks);


cap_id_t cap = 0;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}


void init_server()
{

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
    }
    debug("[SDMMC] create vfs returned %d\n", vfs);
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
}


void main()
{    
    debug("MMC Driver v.0.0.1\n");
    init_server();
    init_mmc();

    /* main IPC loop */
    while (1) {
        
        struct sdmmc_msg inmsg;
        memset(&inmsg, 0, sizeof(inmsg));
        int reply_cap = ipc_receive(cap, &inmsg, sizeof(inmsg), 0);
        struct sdmmc_msg outmsg;
        memset(&outmsg, 0, sizeof(outmsg));

        switch (inmsg.type) {
        case MMC_OP_OPEN: {
            outmsg.type = MMC_OP_REPLY;
            outmsg.msg.open.desc = open_dev(inmsg.msg.open.name, inmsg.msg.open.buf_cap,
                                             inmsg.msg.open.buf_size);
            ipc_reply(reply_cap, &outmsg, sizeof(outmsg));
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
                outmsg.msg.read.blocks = read_dev(inmsg.msg.read.desc, inmsg.msg.read.start, inmsg.msg.read.blocks);
                outmsg.type = MMC_OP_REPLY;
                ipc_reply(reply_cap, &outmsg, sizeof(outmsg));             
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
            outmsg.type = MMC_OP_INVALID;
            ipc_reply(reply_cap, &outmsg, sizeof(outmsg));
            break;
        }
    } /* main loop */
    /* never reached, but cleanup if ever needed */
//    maybe_unsubscribe_irq();
    
}
