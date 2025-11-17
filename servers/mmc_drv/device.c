#include <stdint.h>
#include <string.h>
#include <mosstd.h>
#include <errno.h>
#include <ipc.h>
#include <part.h>

#include "mmc.h"

#define DESC_MAX    32
struct desc_entry {
    int      dev_id;
    int      buf_cap;
    char     *buf;
    uint32_t buf_size;
};

struct device
{    
    uint64_t start;
    uint64_t end;
    const char *name;
};


static struct desc_entry descs[DESC_MAX];

static struct device devices[GPT_ENTRY_NUMBERS+1];



static int desc_do_register(struct desc_entry entry)
{
    for (int i = 0; i < DESC_MAX; i++) {
        if (!descs[i].buf_cap) {
            descs[i].buf_cap  = entry.buf_cap;
            descs[i].buf      = entry.buf;
            descs[i].buf_size = entry.buf_size;
            descs[i].dev_id   = entry.dev_id;
            return i;
        }
    }
    return -ENOSPC;;
}

int lookup_dev(const char *name)
{
    for (int i=0; i< 129; i++) {
        if (!devices[i].name) continue;
        if (strcmp(devices[i].name, name) == 0)
            return i;
    }
    return -ENOENT;
}

int open_dev(const char *name, int buf_cap, uint32_t buf_size)
{
    struct desc_entry e;

    if (!buf_cap || !buf_size || !name) return -EINVAL;
    debug("[MMC] open dev buf_cap %d name %s\n", buf_cap, name);
    int n = lookup_dev(name);
    if (n < 0) return n;
    e.buf_cap  = buf_cap;
    e.buf_size = buf_size;
    e.dev_id = n;
    e.buf = (char *)ipc_shm_attach(buf_cap, NULL, 0);
    if (!e.buf) return -EINVAL;

    return desc_do_register(e);
}

int close_dev(int desc)
{
    if (!descs[desc].buf) return -ENOENT;
    // unmap(descs[desc].buf);
    // cap_free(descs[desc].buf_cap);
    descs[desc].buf_cap  = 0;
    descs[desc].buf      = 0;
    descs[desc].buf_size = 0;
    descs[desc].dev_id   = 0;    
    return SUCCESS;
}

int read_dev(int desc, uint64_t start, uint64_t blocks)
{
    if (!blocks) return -EINVAL;
    if ((blocks * 0x200) > descs[desc].buf_size) return -EINVAL;
    int st = devices[descs[desc].dev_id].start;
    return mmc_bread(descs[desc].buf, st+start-1, blocks);
}

int scan_parts()
{
    gpt_header *gpt;
    gpt_entry *gpt_en;

    // read first block
    gpt = malloc(0x200);
    if (!gpt) return -ENOMEM;

    int n = mmc_bread(gpt, 1, 1);

    if (!n) {
        free(gpt);
        return -EIO;
    }
    if (gpt->signature != GPT_HEADER_SIGNATURE_UBOOT) {
        panic("[MMC DEV] Wrong GPT signature");
    }

//    debug("[MMC DEV] size of GPT entries 0x%X\n", gpt->sizeof_partition_entry*gpt->num_partition_entries);
    gpt_en = malloc(gpt->sizeof_partition_entry*gpt->num_partition_entries);
    if (!gpt) return -ENOMEM;

//    debug("[MMC DEV] GPT: entries at 0x%X\n", gpt_en);
    n = mmc_bread(gpt_en, gpt->partition_entry_lba, gpt->sizeof_partition_entry*gpt->num_partition_entries/0x200);
    if (!n) {
        free(gpt);
        free(gpt_en);
        return -EIO;
    }

    int p_nums = (gpt->num_partition_entries <= GPT_ENTRY_NUMBERS) ? gpt->num_partition_entries : GPT_ENTRY_NUMBERS; 
//    debug("[MMC DEV] GPT: num_partition_entries 0x%X\n", p_nums);
    for (int i=0; i< p_nums; i++) {
        devices[i+1].start = gpt_en[i].starting_lba;
        devices[i+1].end = gpt_en[i].ending_lba;
    }

    free(gpt);
    free(gpt_en);

    return SUCCESS;
}

int init_dev()
{
    memset(devices, 0, sizeof(struct device));
    uint64_t s = get_card_capacity();
    if (!s) return -EIO;
    devices[0].start = 1;
    devices[0].end = s / 0x200;
    debug("[MMC DEV] lba end 0x%lX\n", devices[0].end);
    if (scan_parts() < 0) return -EIO;

    for (int i=1; i< GPT_ENTRY_NUMBERS+1; i++) {
        if (devices[i].start && devices[i].start) ;

    }
    return SUCCESS;
}

uint64_t get_device_start(int n)
{
    return devices[n].start;
}

uint64_t get_device_end(int n)
{
    return devices[n].start;
}

uint64_t get_device_blocks_by_desc(int d)
{
    int i = descs[d].dev_id;
    return devices[i].end - devices[i].start;
}

void set_device_name(int n, const char *name)
{
    devices[n].name = name;
}