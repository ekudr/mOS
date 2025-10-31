#include <common.h>
//#include <libsys/ipc.h>
//#include <memory.h>
#include <string.h>
#include <riscv.h>

#include "syscall.h"
#include <libsys/part.h>
#include <sys/types.h>
#include <mosstd.h>
#include <cap.h>
#include <ipc.h>
#include <nameserver.h>
#include <vfs.h>

#include <devman.h>
#include <tty.h>

#include <mmc_ipc.h>

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

struct stream{
    uint64_t flag;
    char buf[248];
};

struct stream *buffer;

struct disk_decriptor
{
    int disk_cap;
    int disk_desc;
    int buf_cap;
    uint32_t buf_size;
    void *buf;
};

struct disk_decriptor disk;

int disk_open(const char* name, struct disk_decriptor *disk)
{
    int sd_cap;
    do {
        sd_cap = vfs_open(name);
    } while (sd_cap<=0);

    disk->disk_cap = sd_cap;

    disk->buf_cap = cap_shmem_create(disk->buf_size, CRIGHT_GRANT | CRIGHT_MAP);
    if (disk->buf_cap < 0)
        panic("[ONE] Cannot create shared mem cap");

    disk->buf = ipc_shm_attach(disk->buf_cap, NULL, 0);
    if (!disk->buf) panic("[ONE] Cannot attach shared mem cap");

    int tr_cap = cap_transfer(disk->buf_cap, disk->disk_cap, CRIGHT_MAP);
    if (tr_cap < 0) panic("[ONE] Cannot transfer buf cap");

    struct sdmmc_msg sd_msg, sd_rpl;
    memset(&sd_msg, 0, sizeof(sd_msg));

    sd_msg.type = MMC_OP_OPEN;
    sd_msg.msg.open.buf_cap = tr_cap;
    sd_msg.msg.open.buf_size = disk->buf_size;
    ipc_call(disk->disk_cap, &sd_msg, &sd_rpl, sizeof(struct sdmmc_msg));
    if (sd_rpl.type != MMC_OP_REPLY)
        panic("[ONE wrong reply]");

    disk->disk_desc = sd_rpl.msg.open.desc;
    debug("[ONE] SD device descriptor 0x%lX\n", disk->disk_desc);

    return SUCCESS;
}

int disk_read(struct disk_decriptor *disk, uint64_t start, uint64_t blocks)
{
    struct sdmmc_msg sd_msg, sd_rpl;

    if (blocks * 512 > disk->buf_size) return -EINVAL;

    memset(&sd_msg, 0, sizeof(sd_msg));

    sd_msg.type = MMC_OP_READ_BLOCK;
    sd_msg.msg.read.desc = disk->disk_desc;
    sd_msg.msg.read.start = start;
    sd_msg.msg.read.blocks = blocks;
    ipc_call(disk->disk_cap, &sd_msg, &sd_rpl, sizeof(struct sdmmc_msg));
    if (sd_rpl.type != MMC_OP_REPLY)
        panic("[ONE wrong reply]");

    return SUCCESS;
}

cap_id_t buf_cap;

int main()
{
    // uint64_t qkey   = 0x01204E4F43535953;
    // uint64_t qid    = 0;
    // dm_msg_t *msg;
    // struct tty_message cout;
    //uint64_t pid;

    uint64_t next_lba;
    int err;

    // char *hello = "What a hek\n";
    // char *hello_ipc = "Message over IPC\n";
    // pid = getpid();

    cons_out("One App test\n");

    // set buffer size for disk operations
    memset(&disk, 0, sizeof(disk));
    disk.buf_size = 0x4000;
    // open disk
    disk_open("/dev/sdmmc0", &disk);
    disk_read(&disk, 1, 1);

    gpt_header *gpt;
    gpt_entry *gpt_en;

    gpt = (gpt_header *)disk.buf;

    if (gpt->signature != GPT_HEADER_SIGNATURE_UBOOT) {
        panic("[ONE] Wrong GPT signature");
    }
    cons_out("[ONE] GPT: revision 0x%X\n", gpt->revision);
    cons_out("[ONE] GPT: GUID ");
    for (int i = 0; i < 16; i++) {
        cons_out("%X ", gpt->disk_guid.b[i]);
    }
    cons_out("\n");
    cons_out("[ONE] GPT: partition entry lba 0x%lX\n", gpt->partition_entry_lba);
    cons_out("[ONE] GPT: num partition entries %d\n", gpt->num_partition_entries);
    cons_out("[ONE] GPT: size of partition entry %d\n", gpt->sizeof_partition_entry);

    next_lba = gpt->partition_entry_lba;

    cons_out("[ONE] Reading %d bytes for gpt entries\n",gpt->sizeof_partition_entry*gpt->num_partition_entries);
    err = disk_read(&disk, next_lba, gpt->sizeof_partition_entry*gpt->num_partition_entries/0x200);
    if (err < 0) panic("[ONE] read err");
    gpt_en = (gpt_entry *)disk.buf;

    efi_char16_t bootfs_name[] = {'b', 'o', 'o', 't', 'f', 's', 0x00};

    for (int i=0; i< 7/*gpt->num_partition_entries*/; i++) {
 //       if(gpt_en[i].partition_type_guid.b[0]) { 
            cons_out("[ONE] partition %d type GUID ", i);
            for (int j = 0; j < 16; j++) {
                cons_out("%X ", gpt_en[i].partition_type_guid.b[j]);
            }
            cons_out("\n");
            cons_out("[ONE] unique GUID ");
            for (int i = 0; i < 16; i++) {
                cons_out("%X ", gpt_en[i].unique_partition_guid.b[i]);
            }
            cons_out("\n");            
            cons_out("[ONE] GPT: partition %i lbas 0x%lX -> 0x%lX\n", i, gpt_en[i].starting_lba, gpt_en[i].ending_lba);
            cons_out("[ONE] Name ");
            for (int j = 0; j < 36; j++) {
                if (gpt_en[i].partition_name[j])
                    cons_out("%s", gpt_en[i].partition_name + j);
            }
            cons_out("\n");  
            if(!memcmp(gpt_en[i].partition_name, bootfs_name, 7)) {
                // boot_disk.fat_lba = gpt_en[i].starting_lba;
                // // First Sector of partition 
                // ext_fs.start_sect = gpt_en[i].starting_lba;
                // // Total Sector of partition
                // ext_fs.total_sect = gpt_en[i].ending_lba - gpt_en[i].starting_lba;
                // // Block size  of partition 
                // ext_fs.blksz = 512;
                cons_out("[ONE] partition start %d total sectors %d\n", gpt_en[i].starting_lba, gpt_en[i].ending_lba - gpt_en[i].starting_lba);
            }
//        }
    }

//    cap_free(buf_cap);

//     do {
//        qid = shmget(qkey, 0, IPC_EXIST);
//     } while (!qid);

// //    debug("Shmem ID 0x%lX\n", qid);
//     buffer = (struct stream *)shmat(qid, NULL, 0);
// //    debug("Shmem addr 0x%lX\n", buffer);
    
//     for (int i = 0; i < 0x20; i++){
//         uint64_t f = __atomic_exchange_n (&buffer[i].flag, 1, __ATOMIC_ACQ_REL);
//         if (f == 0){
//             strcpy(buffer[i].buf, hello);
//             break;
//         }
//     }

    for(;;);
}