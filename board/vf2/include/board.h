#ifndef __BOARD_H__
#define __BOARD_H__

#define __JH7110__

#include "jh7110_mmap.h"

#define NCPUS   4
#define HARTID2CPU(id) ((id)-1)
#define CPU2HARTID(id) ((id)+1)

#define NIRQS    136 // Num interrupts

#define CONFIG_SYS_TIMER_RATE 4000000
#define TIMER_INTERVAL  10000

#define KLOADADDR   (0x40200000##ULL)
#define MEMMAP_TOP  (0x240000000##ULL)




#endif /* __BOARD_H__ */