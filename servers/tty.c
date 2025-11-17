#include <common.h>
#include <libsys/ipc.h>
//#include <memory.h>
#include <string.h>
#include <riscv.h>

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
    int reg_shift;
    int irq;
    int clk_div;
} uart_dev;

static inline void 
uart_regw(int reg, uint8_t val) {
    putreg8(val, (uint64_t)((uint64_t)uart_dev.base+(reg<<uart_dev.reg_shift)));
}

static inline uint8_t 
uart_regr(int reg){
    return getreg8((uint64_t)((uint64_t)uart_dev.base+(reg<<uart_dev.reg_shift)));
}

struct stream{
    uint64_t flag;
    char buf[248];
};

struct stream *buffer;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

void
uart_putc(char ch)
{
    while ((uart_regr(UART_LSR) & UART_LSR_BUF_EMPTY_MASK) == 0);
    if(ch == '\n')
        uart_regw(UART_THR, '\r');
    uart_regw(UART_THR, ch);
}

// read one input character from the UART.
// return -1 if none is waiting.
int uart_getc(void) {
  if(uart_regr(UART_LSR) & 0x01){
    // input data is ready.
    return uart_regr(UART_RHR);
  } else {
    return -1;
  }
}

void
uart_puts(char *s) {  
    while (*s) {
        uart_putc(*s++);
    }
}

int
uart_init(void)
{
    uart_dev.base = (uintptr_t)mmap(NULL, 4096, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)UART0);
    if(uart_dev.base == 0)
        panic("[UART] init error");

    
    uart_dev.reg_shift = UART0_REG_SHIFT;
    uart_dev.irq = UART0_IRQ;
    uart_dev.clk_div = UART0_DIV;

//    debug("\x1b[31mDEBUG\x1b[0m 0x%lX\n", uart_dev.base);

        // wait transmitter empty
    while ((uart_regr(UART_LSR) & UART_LSR_EMPTY_MASK) == 0);
    

    // disable interrupts.
    uart_regw(UART_IER, 0);

    // and set word length to 8 bits, no parity.
    uart_regw(UART_LCR, UART_LCR_EIGHT_BITS);

    // special mode to set baud rate.
    uart_regw(UART_LCR, UART_LCR_BAUD_LATCH);

    // LSB for baud rate of 115.2K.
    uart_regw(0, uart_dev.clk_div);
    // MSB for baud rate of 115.2K.
    uart_regw(1, 0x00);

    // leave set-baud mode,
    // and set word length to 8 bits, no parity.
    uart_regw(UART_LCR, UART_LCR_EIGHT_BITS);

#if defined(__SPACEMIT_K1__) 
    // Enabling interrupts
    // OUT2 Signal Control.
    // OUT2 connects the UART interrupt output to the interrupt controller unit. When <Loopback Mode> is clear.
    uint8_t msr = uart_regr(UART_MSR);
    uart_regw(UART_MSR, (msr |= UART_MSR_OUT2));
#endif
    // reset and enable FIFOs.
    uart_regw(UART_FCR, UART_FCR_FIFO_ENABLE);
    uart_regw(UART_FCR, UART_FCR_FIFO_ENABLE | UART_FCR_FIFO_CLEAR);

	/*
	 * Clear the interrupt registers.
	 */
	(void) uart_regr(UART_LSR);
	(void) uart_regr(UART_RHR);
	(void) uart_regr(UART_ISR);
	(void) uart_regr(UART_MSR);

    // enable unit.
    uart_regw(UART_IER, UART_INIT_IER);

    // enable transmit and receive interrupts.
    uart_regw(UART_IER, uart_regr(UART_IER) | /*UART_IER_TX_ENABLE |*/ UART_IER_RX_ENABLE);

 

	/*
	 * Clear the interrupt registers.
	 */
	(void) uart_regr(UART_LSR);
	(void) uart_regr(UART_RHR);
	(void) uart_regr(UART_ISR);
	(void) uart_regr(UART_MSR);

    irqhand.handler = irq_handler;
    signal_action(1, &irqhand);

    irq_set(UART0_IRQ, 0);
    return 0;
}
/*
dm_device_t tty_dev = {
    .type = D_TTY,
    .name = "tty0",
};
*/
uint64_t pid;   // Global PID of the task

void irq_handler(uint32_t sig, uint64_t irq)
{
//    debug("[TASK%d] irq handler sig %d irq %d\n", pid, sig, irq);
    while (uart_regr(UART_LSR) & UART_LSR_RX_READY)
    {
        uart_putc(uart_getc());
    }
    irq_act(UART0_IRQ, 0);

}

void init_server(void)
{
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    debug("TTY driver created cap %d\n", cap);

//    struct dm_register tty;
 
    int ret = devman_register("tty0", cap);    
//    debug("[TTY] Register status %d\n", ret);
    if (ret < 0) {
        debug("PANIC TTY");
        for (;;);
    }

    int vfs = vfs_open("/dev/tty0");
    debug("[TTY] open vfs returned %d\n", vfs);
    if (vfs < 0) {
        // ??? transfer cap top vfs
        vfs = vfs_create("/dev/tty0", VFS_DEVICE, cap);
    }
    debug("[TTY] create vfs returned %d\n", vfs);
}

int main()
{
   // uint64_t dmkey  = 0x0152474E4D564544;    // DEVICE MANAGER queue key
    // uint64_t qkey   = 0x01204E4F43535953;
    // uint64_t qid    = 0;
   // uint64_t dmqid  = 0;
   // dm_msg_t *msg;
    
    init_server();

  //  char *hello = "Hello world\r\n";

//    pid = getpid();

//     debug("TTY driver ver. 0.0.1\n");
    
//     qid = shmget(qkey, 0x2000, 0);

//     if (qid == 0)
//         panic("[TTY] shared memory init error");
//     debug("Shmem ID 0x%lX\n", qid);
//     buffer = (struct stream *)shmat(qid, NULL, 0);
//     debug("Shmem addr 0x%lX\n", buffer);
//     memset(buffer, 0, 0x2000);
/*
    do {
       dmqid = get_msg(dmkey, IPC_EXIST);
    } while (!dmqid);

    msg = malloc(sizeof(msg));
    msg->type = DM_REGISTER;
    msg->sender = pid;
    
    memcpy(&msg->msg.device, &tty_dev, sizeof(tty_dev));

    debug("TTY dev type %d name: %s\n", msg->msg.device.type, msg->msg.device.name);

    snd_msg(dmqid, 1, (uintptr_t)msg, sizeof(dm_msg_t), 0);

    rcv_msg(dmqid, pid, (uintptr_t)msg, sizeof(dm_msg_t), 0);

    debug("Response from %d: type %d status %d\n", msg->sender, msg->type, msg->msg.resp);
    
    debug("Size of stream 0x%lX\n", sizeof(struct stream));
*/
    if (uart_init() != 0)
        panic("[UART] init error");  

    for(;;){
        struct tty_message *msg;
        uint64_t info, sender;
        msg = (struct tty_message *)get_ipc_buffer()->msg;
//        memset(msg, 0, sizeof(msg));
        info = ipc_nb_recv(cap, &sender);  
        if((int)(label_from_msginfo_word(info)) < 0) panic("[TTY] info error");
//debug("\x1b[31m0x%lX\x1b[0m", label_from_msginfo_word(info)); 
        if (msg->type == TTY_PUT_STRING) uart_puts(msg->message);
        // ipc_setMR(0, 0x55);
        // _ipc_reply(msginfo_word_new(0,1,0,0));
    //    debug("\x1b[31m[TTY]\x1b[0m msg->type %d\n", msg->type);  
     //   debug("[TTY] received info 0x%lX\n", info);
     //   memcpy(&msg, get_ipc_buffer()->msg, sizeof(msg));

    //    if (!(ipc_receive(cap, &msg, sizeof(msg), IPC_NOWAIT) < 0)) {
           
    //    }
            
        // for (int i = 0; i < sizeof(msg->message); i++) {
        //     if (msg->message[i]) uart_putc(msg->message[i]);
        // }

        // for (int i=0; i<0x20; i++){
        //     if (buffer[i].flag > 0){
        //         uart_puts(buffer[i].buf);
        //         __atomic_store_n(&buffer[i].flag, 0, __ATOMIC_ACQ_REL);
        //     }
                
        // }
    }


    return 0;
}