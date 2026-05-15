#ifndef __BOARD_KRNLDATA_H__
#define __BOARD_KRNLDATA_H__

#define BOARD_SPARSEMEM

#include <memory.h>



#ifdef __BOARD_MEMMAP__
struct board_mmap board_memmap[] = {
    { .base = 0x0, .top = 0x676FFFFF, .type = MEMORY},
    { .base = 0x67700000, .top = 0x7F6FFFFF, .type = MEM_CMA},
    { .base = 0x7F700000, .top = 0x7FFFFFFF, .type = FRMBUF},
    { .base = 0x80000000, .top = 0xFFFFFFFF, .type = MEM_IO},    
    { .base = 0x100000000, .top = 0x17FFFFFFF, .type = MEM_EXT},
    
};
#endif

#endif /* __BOARD_KRNLDATA_H__ */