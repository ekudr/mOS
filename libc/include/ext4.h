#ifndef __EXT4_H__
#define __EXT4_H__

#include <stdint.h>


#define SECTOR_SIZE		0x200
#define LOG2_SECTOR_SIZE	9
#define SUPERBLOCK_START	(2 * 512)
#define SUPERBLOCK_SIZE	1024

// Magic value used to identify an ext2 filesystem.
#define	EXT2_MAGIC			0xEF53
/* Amount of indirect blocks in an inode.  */
#define INDIRECT_BLOCKS			12
/* Maximum lenght of a pathname.  */
#define EXT2_PATH_MAX				4096
/* Maximum nesting of symlinks, used to prevent a loop.  */
#define	EXT2_MAX_SYMLINKCNT		8
/* Maximum file name length */
#define EXT2_NAME_LEN 255

/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV	0	/* The good old (original) format */
#define EXT2_DYNAMIC_REV	1	/* V2 format w/ dynamic inode sizes */

#define EXT2_GOOD_OLD_INODE_SIZE 128

/* Filetype used in directory entry.  */
#define	FILETYPE_UNKNOWN		0
#define	FILETYPE_REG			1
#define	FILETYPE_DIRECTORY		2
#define	FILETYPE_SYMLINK		7

/* Filetype information as used in inodes.  */
#define FILETYPE_INO_MASK		0170000
#define FILETYPE_INO_REG		0100000
#define FILETYPE_INO_DIRECTORY		0040000
#define FILETYPE_INO_SYMLINK		0120000
#define EXT2_ROOT_INO			2 /* Root inode */
#define EXT2_BOOT_LOADER_INO		5 /* Boot loader inode */

/* First non-reserved inode for old ext2 filesystems */
#define EXT2_GOOD_OLD_FIRST_INO	11


/* The size of an ext2 block in bytes.  */
#define EXT2_BLOCK_SIZE(data)	   (1 << LOG2_BLOCK_SIZE(data))

/* Log2 size of ext2 block in bytes.  */
#define LOG2_BLOCK_SIZE(data)	   (data->sblock.log2_block_size + EXT2_MIN_BLOCK_LOG_SIZE)

#define EXT2_FT_DIR	2
#define SUCCESS	1

/* Macro-instructions used to manage several block sizes  */
#define EXT2_MIN_BLOCK_LOG_SIZE	10 /* 1024 */
#define EXT2_MAX_BLOCK_LOG_SIZE	16 /* 65536 */
#define EXT2_MIN_BLOCK_SIZE		(1 << EXT2_MIN_BLOCK_LOG_SIZE)
#define EXT2_MAX_BLOCK_SIZE		(1 << EXT2_MAX_BLOCK_LOG_SIZE)

#define EXT4_INDEX_FL		0x00001000 /* Inode uses hash tree index */
#define EXT4_TOPDIR_FL		0x00020000 /* Top of directory hierarchies*/
#define EXT4_EXTENTS_FL		0x00080000 /* Inode uses extents */
#define EXT4_EXT_MAGIC			0xf30a
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM	0x0010
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM 0x0400
#define EXT4_FEATURE_INCOMPAT_EXTENTS	0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT	0x0080
#define EXT4_INDIRECT_BLOCKS		12

#define EXT4_BG_INODE_UNINIT		0x0001
#define EXT4_BG_BLOCK_UNINIT		0x0002
#define EXT4_BG_INODE_ZEROED		0x0004


struct ext2_sblock {
    uint32 total_inodes;
    uint32 total_blocks;
    uint32 reserved_blocks;
	uint32 free_blocks;
	uint32 free_inodes;
	uint32 first_data_block;
	uint32 log2_block_size;
	uint32 log2_fragment_size;
	uint32 blocks_per_group;
	uint32 fragments_per_group;
	uint32 inodes_per_group;
	uint32 mtime;
	uint32 utime;
	uint16 mnt_count;
	uint16 max_mnt_count;
	uint16 magic;
	uint16 fs_state;
	uint16 error_handling;
	uint16 minor_revision_level;
	uint32 lastcheck;
	uint32 checkinterval;
	uint32 creator_os;
	uint32 revision_level;
	uint16 uid_reserved;
	uint16 gid_reserved;
	uint32 first_inode;
	uint16 inode_size;
	uint16 block_group_number;
	uint32 feature_compatibility;
	uint32 feature_incompat;
	uint32 feature_ro_compat;
	uint32 unique_id[4];
	char volume_name[16];
	char last_mounted_on[64];
	uint32 compression_info;
	uint8 prealloc_blocks;
	uint8 prealloc_dir_blocks;
	uint16 reserved_gdt_blocks;
	uint8 journal_uuid[16];
	uint32 journal_inode;
	uint32 journal_dev;
	uint32 last_orphan;
	uint32 hash_seed[4];
	uint8 default_hash_version;
	uint8 journal_backup_type;
	uint16 descriptor_size;
	uint32 default_mount_options;
	uint32 first_meta_block_group;
	uint32 mkfs_time;
	uint32 journal_blocks[17];
	uint32 total_blocks_high;
	uint32 reserved_blocks_high;
	uint32 free_blocks_high;
	uint16 min_extra_inode_size;
	uint16 want_extra_inode_size;
	uint32 flags;
	uint16 raid_stride;
	uint16 mmp_interval;
	uint64 mmp_block;
	uint32 raid_stripe_width;
	uint8 log2_groups_per_flex;
	uint8 checksum_type;
};

struct ext2_block_group {
	uint32 block_id;	/* Blocks bitmap block */
	uint32 inode_id;	/* Inodes bitmap block */
	uint32 inode_table_id;	/* Inodes table block */
	uint16 free_blocks;	/* Free blocks count */
	uint16 free_inodes;	/* Free inodes count */
	uint16 used_dir_cnt;	/* Directories count */
	uint16 bg_flags;
	uint32 bg_exclude_bitmap;
	uint16 bg_block_id_csum;
	uint16 bg_inode_id_csum;
	uint16 bg_itable_unused; /* Unused inodes count */
	uint16 bg_checksum;	/* crc16(s_uuid+group_num+group_desc)*/
	/* following fields only exist if descriptor size is 64 */
	uint32 block_id_high;
	uint32 inode_id_high;
	uint32 inode_table_id_high;
	uint16 free_blocks_high;
	uint16 free_inodes_high;
	uint16 used_dir_cnt_high;
	uint16 bg_itable_unused_high;
	uint32 bg_exclude_bitmap_high;
	uint16 bg_block_id_csum_high;
	uint16 bg_inode_id_csum_high;
	uint32 bg_reserved;
};

/* The ext2 inode. */
struct ext2_inode {
	uint16 mode;
	uint16 uid;
	uint32 size;
	uint32 atime;
	uint32 ctime;
	uint32 mtime;
	uint32 dtime;
	uint16 gid;
	uint16 nlinks;
	uint32 blockcnt;	/* Blocks of either 512 or block_size bytes */
	uint32 flags;
	uint32 osd1;
	union {
		struct datablocks {
			uint32 dir_blocks[INDIRECT_BLOCKS];
			uint32 indir_block;
			uint32 double_indir_block;
			uint32 triple_indir_block;
		} blocks;
		char symlink[60];
		char inline_data[60];
	} b;
	uint32 version;
	uint32 acl;
	uint32 size_high;	/* previously dir_acl, but never used */
	uint32 fragment_addr;
	uint32 osd2[3];
};

/* The header of an ext2 directory entry. */
struct ext2_dirent {
	uint32 inode;
	uint16 direntlen;
	uint8 namelen;
	uint8 filetype;
};

struct ext2fs_node {
	struct ext2_data *data;
	struct ext2_inode inode;
	int ino;
	int inode_read;
};

/* Information about a "mounted" ext2 filesystem. */
struct ext2_data {
	struct ext2_sblock sblock;
	struct ext2_inode *inode;
	struct ext2fs_node diropen;
};

struct ext_filesystem {
    // First Sector of partition 
    uint64_t start_sect;
	// Total Sector of partition
	uint64_t total_sect;
	// Block size  of partition 
	uint32_t blksz;
	// Inode size of partition 
	uint32_t inodesz;
	// Sectors per Block
	uint32_t sect_perblk;
	// Group Descriptor size 
	uint16_t gdsize;
	// Group Descriptor Block Number
	uint32_t gdtable_blkno;
	// Total block groups of partition 
	uint32_t no_blkgrp;
	// No of blocks required for bgdtable 
	uint32_t no_blk_pergdt;
	// Superblock
	struct ext2_sblock *sb;
	// Block group descritpor table 
	char *gdtable;

	// Block Bitmap Related 
	unsigned char **blk_bmaps;
	long int curr_blkno;
	uint16_t first_pass_bbmap;

	// Inode Bitmap Related 
	unsigned char **inode_bmaps;
	int curr_inode_no;
	uint16_t first_pass_ibmap;

	// Journal Related 

	// Block Device Descriptor 
	// struct blk_desc *dev_desc;
};

/*
 * ext4_inode has i_block array (60 bytes total).
 * The first 12 bytes store ext4_extent_header;
 * the remainder stores an array of ext4_extent.
 */

/*
 * This is the extent on-disk structure.
 * It's used at the bottom of the tree.
 */
struct ext4_extent {
	uint32	ee_block;	/* first logical block extent covers */
	uint16	ee_len;		/* number of blocks covered by extent */
	uint16	ee_start_hi;	/* high 16 bits of physical block */
	uint32	ee_start_lo;	/* low 32 bits of physical block */
};

/*
 * This is index on-disk structure.
 * It's used at all the levels except the bottom.
 */
struct ext4_extent_idx {
	uint32	ei_block;	/* index covers logical blocks from 'block' */
	uint32	ei_leaf_lo;	/* pointer to the physical block of the next *
				 * level. leaf or next index could be there */
	uint16	ei_leaf_hi;	/* high 16 bits of physical block */
	uint16	ei_unused;
};

/* Each block (leaves and indexes), even inode-stored has header. */
struct ext4_extent_header {
	uint16	eh_magic;	/* probably will support different formats */
	uint16	eh_entries;	/* number of valid entries */
	uint16	eh_max;		/* capacity of store in entries */
	uint16	eh_depth;	/* has tree real underlying blocks? */
	uint32	eh_generation;	/* generation of the tree */
};

struct ext_block_cache {
	char *buf;
	uint64_t block;
	int size;
};

int ext4fs_devread(block_dev_t *disk, uint64_t sector, int byte_offset, int byte_len, char *buffer) ;
int ext4fs_read_file(block_dev_t *disk, struct ext2fs_node *node, uint64_t pos,
		    uint64_t len, char *buf, uint64_t *actread);
int ext_cache_read(block_dev_t *disk, struct ext_block_cache *cache, uint64_t block, int size) ;
int ext4_read_file(block_dev_t *disk, const char *filename, void *buf, uint64_t offset, uint64_t len, uint64_t *len_read);
int ext4fs_mount(block_dev_t *disk);
int ext4fs_ls(block_dev_t *disk, const char *dirname) ;
int ext4fs_size(block_dev_t *disk, const char *filename, uintptr_t *size);
#endif /* __EXT4_H__ */