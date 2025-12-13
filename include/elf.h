#ifndef __ELF_H__
#define __ELF_H__

// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint32_t magic;  // must equal ELF_MAGIC
  uint8_t elf[12];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};

typedef struct elfhdr elfhdr_t;

// Program section header
struct proghdr {
  uint32_t type;
  uint32_t flags;
  uint64_t off;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
};

typedef struct proghdr proghdr_t;

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4


inline proghdr_t *__elf_get_phtable(const char *file)
{
    return (void *)file + ((elfhdr_t *)file)->phoff;
}

inline uint64_t __elf_get_entry(const char *file)
{
    return ((elfhdr_t *)file)->entry;
}

inline uint16_t __elf_get_phnum(const char *file)
{
    return ((elfhdr_t *)file)->phnum;
}



#endif  /* __ELF_H__ */