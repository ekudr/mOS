#ifndef __MMC_IPC_H__
#define __MMC_IPC_H__

/* -------------------- IPC opcodes (shared with clients) ------------------ */

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
            int      buf_cap;
            uint32_t buf_size;
            int      desc;    
        } open;
        struct {
            int desc;
            uint64_t start;
            uint64_t blocks;
        } read;
        
    } msg;
    

};


#endif /* __MMC_IPC_H__ */