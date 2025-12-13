#include <common.h>
#include <libsys/ipc.h>
#include <ipc.h>
#include <string.h>
#include <riscv.h>

#include "syscall.h"
#include <part.h>
#include <sys/types.h>
#include <mosstd.h>
#include <libsys/cap.h>

#include <nameserver.h>
#include <vfs.h>

#include <devman.h>
#include <tty.h>

#include <cap.h>
#include <block_dev.h>
#include <ext4.h>


int elf_check_magic(const char *file);
void elf_dump(const char *file);
uint64_t elf_get_entry(const char *file);
uint16_t elf_get_phnum(const char *file);
uint32_t elf_get_ph_type(const char *file, size_t ph);
uint64_t elf_get_ph_paddr(const char *file, size_t ph);
uint64_t elf_get_ph_vaddr(const char *file, size_t ph);
uint64_t elf_get_ph_mem_size(const char *file, size_t ph);
uint64_t elf_get_ph_flags(const char *file, size_t ph);
int elf_load_segment(const char *file, size_t ph, void *buf);

block_dev_t disk;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

cap_id_t buf_cap;

int get_bootfs_part()
{
    int err;

    uint64_t next_lba;

    // set buffer size for disk operations
    memset(&disk, 0, sizeof(disk));
    disk.buf_size = 0x4000;
    // open disk
    do {
        err = disk_open("/dev/sdmmc0", &disk);
    } while(err < 0);
    

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

    next_lba = gpt->partition_entry_lba;

    err = disk_read(&disk, next_lba, gpt->sizeof_partition_entry*gpt->num_partition_entries/0x200);
    if (err < 0) panic("[ONE] read err");
    gpt_en = (gpt_entry *)disk.buf;

    efi_char16_t bootfs_name[] = {'b', 'o', 'o', 't', 'f', 's', 0x00};

    for (int i=0; i< 7/*gpt->num_partition_entries*/; i++) {
        if(!memcmp(gpt_en[i].partition_name, bootfs_name, 7)) {
            cons_out("[ONE] partition %d start 0x%lX total sectors 0x%lX\n", i, gpt_en[i].starting_lba, gpt_en[i].ending_lba - gpt_en[i].starting_lba);
            return i;
        }

    }
    return -ENOENT;
}

int main()
{

    int err;
    uintptr_t fsize;

    cons_out("One App test\n");

    int boot_part = get_bootfs_part();
    if (boot_part < 0) panic("[ONE] bootfs partition not found");

    char name[32];

    err = disk_close(&disk);
    if (err < 0) panic("[ONE] disk close error");

    snprintf_(name, 32, "/dev/sdmmc0p%d", boot_part);
    disk.buf_size = 0x10000;
    disk_open(name, &disk);

    // disk_read(&disk, 3, 1);
    // char *buf = (char *)disk.buf;
    // for (int i=0; i<512; i++){
    //     debug("%x ", buf[i]);
    // }

    ext4fs_mount(&disk);
    ext4fs_ls(&disk, ".");
    ext4fs_size(&disk, "asd", &fsize);
  
    uint64_t len_read;
    char *bmp = (char *)malloc(fsize);
    err = ext4_read_file(&disk, "asd", bmp, 0, fsize, &len_read);
    cons_out("[ONE] ext4 read %d bytes ret %d\n", len_read, err);
    
    if (elf_check_magic(bmp) < 0) panic("wrong magic");
    elf_dump(bmp);

    // create task cap
    int new_task = create_capability(CAP_TASK, 0);
    cons_out("[ONE] task cap created 0x%lX\n", new_task);

    int mem_cap;
//        char *nn = (char *)0x10000000;
//    task_mem_alloc(new_task, vaddr, size, buf, flags);
    for (int i = 0; i < elf_get_phnum(bmp); i++) {
        if (elf_get_ph_type(bmp, i) != 1)
            continue;
        mem_cap = cap_frame_create((void *)0x10000000, elf_get_ph_mem_size(bmp, i), CRIGHT_MAP);
        memset((void *)0x10000000, 0, PGROUNDUP(elf_get_ph_mem_size(bmp, i)));
 //       debug("[ONE] mem cap id 0x%X\n", mem_cap);
        elf_load_segment(bmp, i, (void *)0x10000000);
 //       move pages to new task
//        debug("\n");
        //  for (int j = 0x1000; j < 0x1100; j++) {
        //      debug("0x%X ", nn[j]);
        //  }

        cap_task_mem_move(new_task, (void *)elf_get_ph_vaddr(bmp, i), mem_cap, elf_get_ph_flags(bmp, i));
    }

    cap_task_run(new_task, elf_get_entry(bmp));
 //   debug("[ONE] entry point 0x%X\n", elf_get_entry(bmp));
    cons_out("[ONE] END OF TASK\n");
    for(;;);
}