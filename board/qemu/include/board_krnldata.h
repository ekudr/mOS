#ifndef __BOARD_KRNLDATA_H__
#define __BOARD_KRNLDATA_H__


#include <memory.h>

#define BASE_PFN    (0x80000ULL)

#ifdef __BOARD_MEMMAP__
struct board_mmap board_memmap[] = {
    {0x0, 0x3FFFFFFF, MEM_IO},    // MEM IO U74 core
    {0x80000000, 0xFFFFFFFF, MEMORY},
};
#endif

#endif /* __BOARD_KRNLDATA_H__ */