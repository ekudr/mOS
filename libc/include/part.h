#ifndef __PART_H__
#define __PART_H__

/*
 * Define __packed if someone hasn't beat us to it.  Linux kernel style
 * checking prefers __packed over __attribute__((packed)).
 */
#ifndef __packed
#define __packed __attribute__((packed))
#endif


#define EFI32_LOADER_SIGNATURE	"EL32"
#define EFI64_LOADER_SIGNATURE	"EL64"

typedef struct {
	u8 b[16];
} efi_guid_t __attribute__((aligned(8)));

#define EFI_GUID(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
	{{ (a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, \
		((a) >> 24) & 0xff, \
		(b) & 0xff, ((b) >> 8) & 0xff, \
		(c) & 0xff, ((c) >> 8) & 0xff, \
		(d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) } }

static inline bool is_equal_guid(efi_guid_t *rguid1, efi_guid_t *rguid2)  {
  return !memcmp(rguid1, rguid2, sizeof(efi_guid_t));
}

#define MSDOS_MBR_SIGNATURE 0xAA55
#define MSDOS_MBR_BOOT_CODE_SIZE 440
#define EFI_PMBR_OSTYPE_EFI 0xEF
#define EFI_PMBR_OSTYPE_EFI_GPT 0xEE

#define GPT_HEADER_SIGNATURE_UBOOT 0x5452415020494645ULL // 'EFI PART'
#define GPT_HEADER_CHROMEOS_IGNORE 0x454d45524f4e4749ULL // 'IGNOREME'

#define GPT_HEADER_REVISION_V1 0x00010000
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1ULL
#define GPT_ENTRY_NUMBERS		128
#define GPT_ENTRY_SIZE			128

#define PARTITION_SYSTEM_GUID \
	EFI_GUID( 0xC12A7328, 0xF81F, 0x11d2, \
		0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B)
#define LEGACY_MBR_PARTITION_GUID \
	EFI_GUID( 0x024DEE41, 0x33E7, 0x11d3, \
		0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F)
#define PARTITION_MSFT_RESERVED_GUID \
	EFI_GUID( 0xE3C9E316, 0x0B5C, 0x4DB8, \
		0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE)
#define PARTITION_BASIC_DATA_GUID \
	EFI_GUID( 0xEBD0A0A2, 0xB9E5, 0x4433, \
		0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)
#define PARTITION_LINUX_FILE_SYSTEM_DATA_GUID \
	EFI_GUID(0x0FC63DAF, 0x8483, 0x4772, \
		0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4)
#define PARTITION_LINUX_RAID_GUID \
	EFI_GUID( 0xa19d880f, 0x05fc, 0x4d3b, \
		0xa0, 0x06, 0x74, 0x3f, 0x0f, 0x84, 0x91, 0x1e)
#define PARTITION_LINUX_SWAP_GUID \
	EFI_GUID( 0x0657fd6d, 0xa4ab, 0x43c4, \
		0x84, 0xe5, 0x09, 0x33, 0xc8, 0x4b, 0x4f, 0x4f)
#define PARTITION_LINUX_LVM_GUID \
	EFI_GUID( 0xe6d6d379, 0xf507, 0x44c2, \
		0xa2, 0x3c, 0x23, 0x8f, 0x2a, 0x3d, 0xf9, 0x28)
#define PARTITION_U_BOOT_ENVIRONMENT \
	EFI_GUID( 0x3de21764, 0x95bd, 0x54bd, \
		0xa5, 0xc3, 0x4a, 0xbe, 0x78, 0x6f, 0x38, 0xa8)

/* linux/include/efi.h */
typedef uint16_t efi_char16_t;

/* based on linux/include/genhd.h */
struct partition {
	uint8_t boot_ind;		/* 0x80 - active */
	uint8_t head;		/* starting head */
	uint8_t sector;		/* starting sector */
	uint8_t cyl;			/* starting cylinder */
	uint8_t sys_ind;		/* What partition type */
	uint8_t end_head;		/* end head */
	uint8_t end_sector;		/* end sector */
	uint8_t end_cyl;		/* end cylinder */
	uint32_t start_sect;	/* starting sector counting from 0 */
	uint32_t nr_sects;	/* nr of sectors in partition */
} __packed;

/* based on linux/fs/partitions/efi.h */
typedef struct _gpt_header {
	uint64_t signature;
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved1;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	efi_guid_t disk_guid;
	uint64_t partition_entry_lba;
	uint32_t num_partition_entries;
	uint32_t sizeof_partition_entry;
	uint32_t partition_entry_array_crc32;
} __packed gpt_header;


typedef union _gpt_entry_attributes {
	struct {
		uint64_t required_to_function:1;
		uint64_t no_block_io_protocol:1;
		uint64_t legacy_bios_bootable:1;
		uint64_t reserved:45;
		uint64_t type_guid_specific:16;
	} fields;
	unsigned long long raw;
} __packed gpt_entry_attributes;

#define PARTNAME_SZ	(72 / sizeof(efi_char16_t))
typedef struct _gpt_entry {
	efi_guid_t partition_type_guid;
	efi_guid_t unique_partition_guid;
	uint64_t starting_lba;
	uint64_t ending_lba;
	gpt_entry_attributes attributes;
	efi_char16_t partition_name[PARTNAME_SZ];
} __packed gpt_entry;

typedef struct _legacy_mbr {
	uint8_t boot_code[MSDOS_MBR_BOOT_CODE_SIZE];
	uint32_t unique_mbr_signature;
	uint16_t unknown;
	struct partition partition_record[4];
	uint16_t signature;
} __packed legacy_mbr;



#endif /* __PART_H__ */