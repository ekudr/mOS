#ifndef __JH7110_MMAP_H__
#define __JH7110_MMAP_H__


// qemu puts UART registers here in physical memory.
#define UART0 0xD4017000L
#define UART0_IRQ 42        //?
#define UART0_REG_SHIFT 2
#define UART0_DIV 0x08
#define UART_INIT_IER 0x40

#define FB_BASE 0x7f700000
#define FB_SIZE	0x900000

/*
 * PLIC Configuration
 */

/* PLIC Base address */
#define PLIC_BASE 0xE0000000L

/* Interrupt Priority */
#define PLIC_PRIORITY  (PLIC_BASE + 0x000000)

/* Hart 1 S-Mode Interrupt Enable */
#define PLIC_ENABLE1   (PLIC_BASE + 0x002100)
#define PLIC_ENABLE2   (PLIC_BASE + 0x002104)

/* Hart 1 S-Mode Priority Threshold */
#define PLIC_THRESHOLD (PLIC_BASE + 0x201000)

/* Hart 1 S-Mode Claim / Complete */
#define PLIC_CLAIM     (PLIC_BASE + 0x201004)

#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2080 + (hart)*0x100)

#define PLIC_SPRIORITY(hart) (PLIC_THRESHOLD + (hart)*0x2000)

#define PLIC_SCLAIM(hart) (PLIC_CLAIM + (hart)*0x2000)



#endif /* __JH7110_MMAP_H__ */