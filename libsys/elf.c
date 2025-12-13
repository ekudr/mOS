#include <stdint.h>
#include <stddef.h>
#include <elf.h>
#include <errno.h>
#include <memory.h>
#include <string.h>

extern int debug(const char *format, ...);

int elf_check_magic(const char *file)
{
    elfhdr_t *elf = (elfhdr_t *)file;
    if (elf->magic != ELF_MAGIC) return -ENOENT;

    return SUCCESS;
}

void elf_dump(const char *file)
{

    for (int i = 0; i < __elf_get_phnum(file); i++)
    {
      
        debug("[ELF] program type 0x%X, mem sz 0x%lX, file sz 0x%lX, Vaddr 0x%lX, Phis addr 0x%lX, file offset 0x%lX, allign 0x%lX\n",
               __elf_get_phtable(file)[i].type, __elf_get_phtable(file)[i].memsz,
               __elf_get_phtable(file)[i].filesz, __elf_get_phtable(file)[i].vaddr,
               __elf_get_phtable(file)[i].paddr, __elf_get_phtable(file)[i].off, __elf_get_phtable(file)[i].align);

          
   }    
}


uint64_t elf_get_entry(const char *file)
{
    return ((elfhdr_t *)file)->entry;
}

uint16_t elf_get_phnum(const char *file)
{
    return __elf_get_phnum(file);
}

uint32_t elf_get_ph_type(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].type;
}

uint64_t elf_get_ph_paddr(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].paddr;
}

uint64_t elf_get_ph_vaddr(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].vaddr;
}

uint64_t elf_get_ph_mem_size(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].memsz;
}

uint64_t elf_get_ph_file_size(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].filesz;
}

uint64_t elf_get_ph_offset(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].off;
}

uint64_t elf_get_ph_flags(const char *file, size_t ph)
{
    return __elf_get_phtable(file)[ph].flags;
}

int elf_load_segment(const char *file, size_t ph, void *buf)
{
    void *addr;
    uint64_t va, s, d, sz;

    va = elf_get_ph_vaddr(file, ph);
    sz = elf_get_ph_file_size(file, ph);
    addr = (void *)file + elf_get_ph_offset(file, ph);
// char * nn = buf;
    s = va % PAGE_SIZE;
    
    if(s != 0) {
        buf += s;
        d = PAGE_SIZE - s;
        d = (sz < d) ? sz : d; 
    
        memcpy(buf, addr, d);
        addr += d;
        sz -= d;
    }
   
    if (sz) memcpy(buf, addr, sz);

//    debug("\n");    

    // for (int j = 0xa08; j < 0xb00; j++) {
    //          debug("0x%X ", nn[j]);
    //      }

    return 0;
}