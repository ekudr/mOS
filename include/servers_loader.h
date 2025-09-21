#ifndef __SERVERS_LOADER_H__
#define __SERVERS_LOADER_H__

#include <stdint.h>

#define SVCSIMG_MAGIC 0x474D495343565253UL


typedef struct services_img_header {
    uint64_t magic;
    uint64_t flags;
    uint64_t nfiles;
    uint64_t hdrsize;
} svcshdr_t;

typedef struct services_img_entry {
    char name[16];
    uint64_t offset;
    uint64_t len;
} svcsent_t;

void load_servers(void);

#endif /* __SERVERS_LOADER_H__ */