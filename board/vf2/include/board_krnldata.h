#ifndef __BOARD_KRNLDATA_H__
#define __BOARD_KRNLDATA_H__

#include <memory.h>

#define BASE_PFN    (0x40000ULL)

#ifdef __BOARD_MEMMAP__
struct board_mmap board_memmap[] = {
    { .base = 0x0, .top = 0x3FFFFFFF, .type = MEM_IO},    // MEM IO U74 core
    { .base = 0x40000000, .top = 0xD5FFFFFF, .type = MEMORY},
    { .base = 0xD6000000, .top = 0xFDFFFFFF, .type = MEM_CMA},
    { .base = 0xFE000000, .top = 0xFFFFFFFF, .type = FRMBUF},
    { .base = 0x100000000, .top = 0x23FFFFFFF, .type = MEM_EXT}
};
#endif

#endif /* __BOARD_KRNLDATA_H__ */