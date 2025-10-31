#include <stdint.h>
#include <string.h>
#include <mosstd.h>
#include <errno.h>
#include <ipc.h>

#include "mmc.h"

#define DESC_MAX    32
struct desc_entry {
    int      dev_id;
    int      buf_cap;
    char     *buf;
    uint32_t buf_size;
};

static struct desc_entry descs[DESC_MAX];



static int desc_do_register(struct desc_entry entry)
{
    for (int i = 0; i < DESC_MAX; i++) {
        if (!descs[i].buf_cap) {
            descs[i].buf_cap  = entry.buf_cap;
            descs[i].buf      = entry.buf;
            descs[i].buf_size = entry.buf_size;
            return i;
        }
    }
    return -ENOSPC;;
}


int open_dev(const char *name, int buf_cap, uint32_t buf_size)
{
    struct desc_entry e;

    if (!buf_cap || !buf_size) return -EINVAL;

    e.buf_cap  = buf_cap;
    e.buf_size = buf_size;

    e.buf = (char *)ipc_shm_attach(buf_cap, NULL, 0);
    if (!e.buf) return -EINVAL;

    return desc_do_register(e);
}

int read_dev(int desc, uint64_t start, uint64_t blocks)
{
    if (!blocks) return -EINVAL;
    if ((blocks * 0x200) > descs[desc].buf_size) return -EINVAL;

    return mmc_bread(descs[desc].buf, start, blocks);
}