#include <mosstd.h>
#include <libsys/ipc.h>
#include <string.h>
#include <block_dev.h>
#include <libsys/cap.h>
#include <vfs.h>
#include <ipc.h>



int disk_open(const char* path, block_dev_t *disk)
{
    int dev_cap;
    
    if (!disk->buf_size) return -EINVAL;

    // open vfs path
    dev_cap = vfs_open(path);
    if (dev_cap < 0) return dev_cap;
    
    disk->disk_cap = dev_cap;

    disk->buf_cap = cap_shmem_create(disk->buf_size,  CRIGHT_GRANT | CRIGHT_MAP);
    if (disk->buf_cap < 0) return disk->buf_cap;

    disk->buf = ipc_shm_attach(disk->buf_cap, NULL, 0);
    // ??? FIXME need right error
    if (!disk->buf) return -ENOMEM;

    struct sdmmc_msg *sd_msg = (struct sdmmc_msg *)get_ipc_buffer()->msg;
    memset(sd_msg, 0, sizeof(*sd_msg));

    // Shrink path to device name
    const char *n = strrchr(path, '/');
    n = n ? n+1 : path;

    strncpy(sd_msg->msg.open.name, n, sizeof(sd_msg->msg.open.name)-1);

    sd_msg->type = MMC_OP_OPEN;
    sd_msg->msg.open.buf_size = disk->buf_size;
    ipc_set_cap(0, disk->buf_cap);

    uint64_t info = msginfo_word_new(0, sizeof(*sd_msg)/8, 1, 0);
    info = ipc_call(disk->disk_cap, info);    

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        return ret;
    }

    if (sd_msg->type != MMC_OP_REPLY)
       return -EINVAL;

    disk->disk_desc     = sd_msg->msg.open.desc;
    disk->total_blocks  = sd_msg->msg.open.total_blocks;
 
    return disk->disk_desc;
}

int disk_close(block_dev_t *disk)
{
    struct sdmmc_msg *sd_msg = (struct sdmmc_msg *)get_ipc_buffer()->msg;
    memset(sd_msg, 0, sizeof(*sd_msg));
    
    sd_msg->type = MMC_OP_CLOSE;

    sd_msg->msg.ctrl.desc = disk->disk_desc;

    uint64_t info = msginfo_word_new(0, sizeof(*sd_msg)/8, 0, 0);
    info = ipc_call(disk->disk_cap, info);  

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        return ret;
    }

    if (sd_msg->type != MMC_OP_REPLY)
        return -EINVAL;

    if (sd_msg->msg.ctrl.desc < 0) return sd_msg->msg.ctrl.desc;

    // unmap_shmem()
 //   cap_free(disk->buf_cap);
    cap_free(disk->disk_cap);

    disk->buf=0;
    disk->buf_cap=0;
    disk->buf_size=0;
    disk->disk_cap=0;
    disk->disk_desc=0;

    return SUCCESS;
}

int disk_read(block_dev_t *disk, uint64_t start, uint64_t blocks)
{
    struct sdmmc_msg *sd_msg = (struct sdmmc_msg *)get_ipc_buffer()->msg;

    if (blocks * 512 > disk->buf_size) return -EINVAL;

    memset(sd_msg, 0, sizeof(*sd_msg));

    sd_msg->type = MMC_OP_READ_BLOCK;
    sd_msg->msg.read.desc = disk->disk_desc;
    sd_msg->msg.read.start = start;
    sd_msg->msg.read.blocks = blocks;

    uint64_t info = msginfo_word_new(0, sizeof(*sd_msg)/8, 0, 0);
    info = ipc_call(disk->disk_cap, info); 
    int ret = (int)label_from_msginfo_word(info);

    if (ret < 0 || !length_from_msginfo_word(info)) {
        return ret;
    }    
    if (sd_msg->type != MMC_OP_REPLY)
        return -EINVAL;

    if (!sd_msg->msg.read.blocks) return -EIO;

    return SUCCESS;
}

int fs_devread(block_dev_t *disk, uint64_t sector, int byte_offset, int byte_len, char *buf) 
{
	uint64_t block_len;
	int log2blksz, err;
    int blksz = 512;
    
//	ALLOC_CACHE_ALIGN_BUFFER(char, sec_buf, (blk ? blk->blksz : 0));
    // char sec_buf[blksz];

	log2blksz = 9; //log2(blksz)
 //   debug("\x1b[31m[READ]\x1b[0m disk read %d bytes\n", byte_len);
//    debug("read %d bytes from sector %d, offset %d\n", byte_len, sector, byte_offset);
//    debug("partition start %d total sectors %d\n", ext_fs.start_sect, ext_fs.total_sect);
	// Check partition boundaries
	if ((sector + ((byte_offset + byte_len - 1) >> log2blksz))
	    >= disk->total_blocks * blksz) {
		debug("[EXT_FS] read outside partition %d\n", sector);
		return EINVAL;
	}

	/* Get the read to the beginning of a partition */
	sector += byte_offset >> log2blksz;
	byte_offset &= blksz - 1;

//	debug(" < %d, %d, %d>\n", sector, byte_offset, byte_len);

	if (byte_offset != 0) {
		int readlen;
		// read first part which isn't aligned with start of sector
        err = disk_read(disk, sector+1, 1);
//        err = boot_disk.mmc->bread((void *)sec_buf, ext_fs.start_sect + sector, 1); 
        if (err < 0) {
            debug("[DISK] ** %s read error %d **\n", __func__, err);
            return err;  
        }
		readlen = min((int)blksz - byte_offset,
			      byte_len);
		memcpy(buf, disk->buf + byte_offset, readlen);
		buf += readlen;
		byte_len -= readlen;
		sector++;
	}

	if (byte_len == 0)
		return SUCCESS;

	// read sector aligned part
	block_len = byte_len & ~(blksz - 1);

	if (block_len == 0) {

		block_len = blksz;

        err = disk_read(disk, sector+1, 1);
        if (err < 0) {
            debug("[EXT_FS] read BOOTFS return %d\n", err);
            return err;  
        }
		memcpy(buf, disk->buf, byte_len);
		return SUCCESS; 
	}

 //   debug("\x1b[31m[READ]\x1b[0m disk read %d blocks\n", block_len >> log2blksz);

    uint64_t read_len;

    while (block_len)
    {

        read_len = (block_len > disk->buf_size) ? disk->buf_size : block_len;
    //    debug("\x1b[31m[READ]\x1b[0m read %d blocks\n", read_len >> log2blksz);        
        err = disk_read(disk, sector+1, read_len >> log2blksz);
        if (err < 0) {
            debug("[EXT_FS] ** %s block read error %d\n", __func__, err);
            return err;  
        }
        memcpy(buf, disk->buf, read_len);

        block_len -= read_len;
        buf += read_len;
        byte_len -= read_len;
        sector += read_len / blksz;
    }
       


	
	// block_len = byte_len & ~(blksz - 1);
	// buf += block_len;
	// byte_len -= block_len;
	// sector += block_len / blksz;

	if (byte_len != 0) {
 //           debug("\x1b[31m[READ]\x1b[0m disk read rest of %d bytes\n", byte_len);
		// read rest of data which are not in whole sector
        err = disk_read(disk, sector+1, 1);
        if (err < 0) {
            debug("[EXT_FS] read return %d\n", err);
            return err;  
        }
		memcpy(buf, disk->buf, byte_len);
	}
	return SUCCESS;
}