#include <common.h>
#include <libsys/ipc.h>
#include <libsys/thread.h>
#include <string.h>
#include <riscv.h>
#include <sched.h>

#include "syscall.h"

//#include <sys/types.h>
#include <mosstd.h>
#include <signals.h>
#include <nameserver.h>
#include <libsys/cap.h>
#include <ipc.h>
#include <vfs.h>
#include <devman.h>
#include <tty.h>

void irq_handler(uint32_t sig, uint64_t irq);
signal_action_t irqhand;

int cap = 0;

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define UART_RHR 0                 // receive holding register (for input bytes)
#define UART_THR 0                 // transmit holding register (for output bytes)
#define UART_IER 1                 // interrupt enable register
#define UART_IER_RX_ENABLE (1<<0)
#define UART_IER_TX_ENABLE (1<<1)
#define UART_IER_UNIT_ENABLE (1<<6)  // Unit Enable bit
#define UART_FCR 2                 // FIFO control register
#define UART_FCR_FIFO_ENABLE (1<<0)
#define UART_FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define UART_ISR 2                 // interrupt status register
#define UART_ISR_INT_PEN  (1<<0)    // interrupt pending
#define UART_LCR 3                 // line control register
#define UART_LCR_EIGHT_BITS (3<<0)
#define UART_LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define UART_MSR 4                 // Modem Control Register
#define UART_MSR_OUT2 (1<<3)
#define UART_LSR 5                 // line status register
#define UART_LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define UART_LSR_TX_IDLE (1<<5)    // THR can accept another character to send


#define UART_LSR_BUF_EMPTY_MASK 0x20  // LSR bit 5 - Transmit Buffer Empty; the UART sent data from the THR to the OSR
#define UART_LSR_EMPTY_MASK 0x40 // LSR bit 6 - Transmitter empty; both the THR and LSR are empty


#ifndef UART_INIT_IER
#define UART_INIT_IER 0x00
#endif

struct uart_dev{
    uintptr_t base;
    uintptr_t th_base;
    int reg_shift;
    int irq;
    int clk_div;
} uart_dev;

static void uart_regw(int reg, uint8_t val, bool th)
{
    if (th) {
        putreg8(val, (uint64_t)((uint64_t)uart_dev.th_base+(reg<<uart_dev.reg_shift)));
    } else {
        putreg8(val, (uint64_t)((uint64_t)uart_dev.base+(reg<<uart_dev.reg_shift)));
    }
    
}

static uint8_t uart_regr(int reg, bool th)
{
    uint8_t ret;
    if (th) {
        ret = getreg8((uint64_t)((uint64_t)uart_dev.th_base+(reg<<uart_dev.reg_shift)));
    } else {
        ret = getreg8((uint64_t)((uint64_t)uart_dev.base+(reg<<uart_dev.reg_shift)));
    }
    return ret;
}

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

void uart_putc(char ch, bool th)
{
    while ((uart_regr(UART_LSR, th) & UART_LSR_EMPTY_MASK /*UART_LSR_BUF_EMPTY_MASK*/) == 0);
    if(ch == '\n')
        uart_regw(UART_THR, '\r', th);
    uart_regw(UART_THR, ch, th);
}

// read one input character from the UART.
// return -1 if none is waiting.
int uart_getc(bool th) 
{
  if(uart_regr(UART_LSR, th) & 0x01){
    // input data is ready.
    return uart_regr(UART_RHR, th);
  } else {
    return -1;
  }
}

void uart_puts(char *s, bool th)
{  
    while (*s) {
        uart_putc(*s++, th);
    }
}

int uart_init(void)
{
    uart_dev.base = (uintptr_t)mmap(NULL, 4096, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)UART0);
    if(uart_dev.base == 0)
        panic("[UART] init error");

//    debug("\x1b[31m[TTY]\x1b[0m MEMIO mapped at 0x%lX\n", uart_dev.base);
    uart_dev.reg_shift = UART0_REG_SHIFT;
    uart_dev.irq = UART0_IRQ;
    uart_dev.clk_div = UART0_DIV;

//    debug("\x1b[31mDEBUG\x1b[0m 0x%lX\n", uart_dev.base);

        // wait transmitter empty
    while ((uart_regr(UART_LSR, false) & UART_LSR_EMPTY_MASK) == 0);
    

    // disable interrupts.
    uart_regw(UART_IER, 0, false);

    // and set word length to 8 bits, no parity.
    uart_regw(UART_LCR, UART_LCR_EIGHT_BITS, false);

    // special mode to set baud rate.
    uart_regw(UART_LCR, UART_LCR_BAUD_LATCH, false);

    // LSB for baud rate of 115.2K.
    uart_regw(0, uart_dev.clk_div, false);
    // MSB for baud rate of 115.2K.
    uart_regw(1, 0x00, false);

    // leave set-baud mode,
    // and set word length to 8 bits, no parity.
    uart_regw(UART_LCR, UART_LCR_EIGHT_BITS, false);

#if defined(__SPACEMIT_K1__) 
    // Enabling interrupts
    // OUT2 Signal Control.
    // OUT2 connects the UART interrupt output to the interrupt controller unit. When <Loopback Mode> is clear.
    uint8_t msr = uart_regr(UART_MSR, false);
    uart_regw(UART_MSR, (msr |= UART_MSR_OUT2), false);
#endif
    // reset and enable FIFOs.
    uart_regw(UART_FCR, UART_FCR_FIFO_ENABLE, false);
    uart_regw(UART_FCR, UART_FCR_FIFO_ENABLE | UART_FCR_FIFO_CLEAR, false);

	/*
	 * Clear the interrupt registers.
	 */
	(void) uart_regr(UART_LSR, false);
	(void) uart_regr(UART_RHR, false);
	(void) uart_regr(UART_ISR, false);
	(void) uart_regr(UART_MSR, false);

    // enable unit.
    uart_regw(UART_IER, UART_INIT_IER, false);

    // enable transmit and receive interrupts.
    uart_regw(UART_IER, uart_regr(UART_IER, false) | /*UART_IER_TX_ENABLE |*/ UART_IER_RX_ENABLE, false);

 

	/*
	 * Clear the interrupt registers.
	 */
	(void) uart_regr(UART_LSR, false);
	(void) uart_regr(UART_RHR, false);
	(void) uart_regr(UART_ISR, false);
	(void) uart_regr(UART_MSR, false);

    return 0;
}

uint64_t pid;   // Global PID of the task

void irq_handler(uint32_t sig, uint64_t irq)
{
//    debug("[TASK%d] irq handler sig %d irq %d\n", pid, sig, irq);
    while (uart_regr(UART_LSR, true) & UART_LSR_RX_READY)
    {
        uart_putc(uart_getc(true), true);
    }
    irq_act(UART0_IRQ, 0);
}

void ipc_handler(void *arg)
{
    // Child maps UART MMIO same addres as parent 
    uart_dev.th_base = (uintptr_t)mmap(NULL, 4096, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)UART0);
    if(!uart_dev.th_base)
        panic("[TTY] Child MMIO mapping error");
//    debug("\x1b[31m[TTY]\x1b[0m Thread MEMIO mapped at 0x%lX\n", uart_dev.th_base);
    irqhand.handler = irq_handler;
    signal_action(1, &irqhand);

    irq_set(UART0_IRQ, 0);

    for(;;){
        sched_yield();
    }
}

void init_server(void)
{
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (cap < 0)
        panic("TTY driver creating cap");

 
    int ret = devman_register("tty0", cap);    
//    debug("[TTY] Register status %d\n", ret);
    if (ret < 0) {
        panic("TTY DEVMAN register");
    }

    int vfs = vfs_open("/dev/tty0");
//    debug("[TTY] open vfs returned %d\n", vfs);
    if (vfs < 0) {
        vfs = vfs_create("/dev/tty0", VFS_DEVICE, cap);
    }
//    debug("[TTY] create vfs returned %d\n", vfs);

    thread_handle_t h;
    int rc = thread_spawn(ipc_handler, NULL, 4096, &h);
    if (rc < 0) {
//        debug("[TTY] thread_spawn returned %d\n", rc);
        panic("spawn failed");
    }
    
}

int main()
{
    debug("TTY driver v. 0.0.4\n");

    if (uart_init() != 0)
        panic("[UART] init error");

    init_server();

    for(;;){
        struct tty_message *msg;
        uint64_t info, sender;
        msg = (struct tty_message *)get_ipc_buffer()->msg;
//        memset(msg, 0, sizeof(msg));
        info = ipc_recv(cap, &sender);  
        if((int)(label_from_msginfo_word(info)) < 0) panic("[TTY] info error");
//debug("\x1b[31m0x%lX\x1b[0m", label_from_msginfo_word(info)); 
        if (msg->type == TTY_PUT_STRING) 
            uart_puts(msg->message, false);
    }
    return 0;
}