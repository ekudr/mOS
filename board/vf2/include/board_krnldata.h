#ifndef __BOARD_KRNLDATA_H__
#define __BOARD_KRNLDATA_H__

#include <memory.h>

#define BASE_PFN    (0x40000ULL)

#ifdef __BOARD_MEMMAP__
struct board_mmap board_memmap[] = {
    {0x0, 0x3FFFFFFF, MEM_IO},    // MEM IO U74 core
    {0x40000000, 0xFDFFFFFF, MEMORY},
    {0xFE000000, 0xFFFFFFFF, FRMBUF},
    {0x100000000, 0x23FFFFFFF, MEM_EXT}
};
#endif

#endif /* __BOARD_KRNLDATA_H__ */