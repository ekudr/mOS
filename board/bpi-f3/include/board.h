#ifndef __BOARD_H__
#define __BOARD_H__

#define __SPACEMIT_K1__

#include "k1-x_mmap.h"
#include "k1-x-pinctrl.h"

#define NCPUS   8
#define HARTID2CPU(id) (id)
#define CPU2HARTID(id) (id)

#define NIRQS    159 // Num interrupts

#define CONFIG_SYS_TIMER_RATE 24000000
#define TIMER_INTERVAL  10000

#define KLOADADDR   (0x200000##ULL)
#define MEMMAP_TOP  (0x180000000##ULL)

#define HEARTBEAT_PIN   DVL0
#define HEARTBEAT_SEL   (MUX_MODE1 | EDGE_NONE | PULL_DOWN | PAD_1V8_DS2)


#endif /* __BOARD_H__ */