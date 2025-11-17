#ifndef __LIBSYS_BLOCK_DEV_H__
#define __LIBSYS_BLOCK_DEV_H__

typedef struct block_dev
{
    int      disk_cap;
    int      disk_desc;
    int      buf_cap;
    uint32_t buf_size;
    void     *buf;
    uint64_t total_blocks;

} block_dev_t;

// ??? FIXME rewrite from MMC to BLOCK

enum mmc_op {
    MMC_OP_GET_INFO = 1,
    MMC_OP_OPEN,
    MMC_OP_CLOSE,
    MMC_OP_READ_BLOCK,
    MMC_OP_WRITE_BLOCK,
    MMC_OP_REPLY,
    MMC_OP_INVALID,
};

struct sdmmc_msg {
    int     type;
    union {
        struct {
            char     name[32];
//            int      buf_cap;
            uint32_t buf_size;
            uint64_t total_blocks;
            int      desc;    
        } open;
        struct {
            int desc;
            uint64_t start;
            uint64_t blocks;
        } read;
        struct {
            int desc;
        } ctrl;
        
    } msg;
    

};



int disk_open(const char* path, block_dev_t *disk);
int disk_close(block_dev_t *disk);
int disk_read(block_dev_t *disk, uint64_t start, uint64_t blocks);
int fs_devread(block_dev_t *disk, uint64_t sector, int byte_offset, int byte_len, char *buf);

#endif /* __LIBSYS_BLOCK_DEV_H__ */
