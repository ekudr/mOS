#ifndef __BOARD_H__
#define __BOARD_H__

#include "qemu_mmap.h"

#define NCPUS   4
#define HARTID2CPU(id) ((id)-1)
#define CPU2HARTID(id) ((id)+1)

#define NIRQS   53 // Num interrupts

#define KLOADADDR   (0x80200000##ULL)
#define MEMMAP_TOP  (0x100000000##ULL)

#define CONFIG_SYS_TIMER_RATE 1000000
#define TIMER_INTERVAL  10000


#endif /* __BOARD_H__ */