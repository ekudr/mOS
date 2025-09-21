#ifndef __BOARD_KRNLDATA_H__
#define __BOARD_KRNLDATA_H__

#define BOARD_SPARSEMEM

#include <memory.h>



#ifdef __BOARD_MEMMAP__
struct board_mmap board_memmap[] = {
    {0x0, 0x7F6FFFFF, MEMORY},
    {0x7F700000, 0x7FFFFFFF, FRMBUF},
    {0x80000000, 0xFFFFFFFF, MEM_IO},    
    {0x100000000, 0x17FFFFFFF, MEM_EXT},
    
};
#endif

#endif /* __BOARD_KRNLDATA_H__ */