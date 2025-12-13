#ifndef __JH7110_MMAP_H__
#define __JH7110_MMAP_H__


// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 32        // Global IRQ + 5
#define UART0_REG_SHIFT 2
#define UART0_DIV 0x0D

#define FB_BASE	0xfe000000UL
#define FB_SIZE	0x2000000UL

/*
 * PLIC Configuration
 */

/* PLIC Base address */
#define PLIC_BASE 0x0c000000L

/* Interrupt Priority */
#define PLIC_PRIORITY  (PLIC_BASE + 0x000000)

/* Hart 1 S-Mode Interrupt Enable */
#define PLIC_ENABLE1   (PLIC_BASE + 0x002100)
#define PLIC_ENABLE2   (PLIC_BASE + 0x002104)

/* Hart 1 S-Mode Priority Threshold */
#define PLIC_THRESHOLD (PLIC_BASE + 0x202000)

/* Hart 1 S-Mode Claim / Complete */
#define PLIC_CLAIM     (PLIC_BASE + 0x202004)

#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2100 + (hart-1)*0x100)

#define PLIC_SPRIORITY(hart) (PLIC_THRESHOLD + (hart-1)*0x2000)

#define PLIC_SCLAIM(hart) (PLIC_CLAIM + (hart-1)*0x2000)



#endif /* __JH7110_MMAP_H__ */