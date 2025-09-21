#include <common.h>
#include <mmu.h>
#include <memory.h>
#include <sched.h>
#include <riscv.h>
#include <trap.h>
#include <servers_loader.h>
#include <ipc.h>
#include <irq.h>

extern uint64_t boot_hartid;
extern struct cpu cpus[NCPUS];

extern uint64_t _imglen;

void plic_init(void);
void sbi_init (void);

void __attribute__((noreturn))
kernel_init(void)
{
    printf("Booting on HART %d\n", boot_hartid);
    // init per-CPU structures before vm_init
    // it needs for spinlock
#ifdef __SPACEMIT_K1__
    boot_hartid--;
#endif    
    current_cpu = &cpus[HARTID2CPU(boot_hartid)];
    current_cpu->hartid = boot_hartid;
    printf("Stack SP 0x%lX\n", r_sp());



    sbi_init();

    vm_init();   // Virtual memory initialization

    current_cpu->stack = kstack_alloc();
    printf("Stack SP 0x%lX\n", r_sp());
    printf("Stack start 0x%lX\n", current_cpu->stack->start);

    // should be reserved stack for this function
    // compiler gives 32 bytes in this time
    // check in other situation
    // !!!REWRITE
    uint64_t sp = current_cpu->stack->start + current_cpu->stack->size - 0x10;
    __asm__ __volatile__("mv sp, %0" : : "r" (sp));
    printf("Stack SP 0x%lX\n", r_sp());

    void trap_init();
    plic_init();
    irq_init();

    plic_cpu_enable(current_cpu->hartid);
    // enable the interrupts
    w_sstatus(r_sstatus() | SSTATUS_SIE);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);


    //Set TIMER
    sbi_set_timer(r_time() + usec_to_tick(TIMER_INTERVAL));
    
    ipc_init();
    sched_init();

    load_servers();

    scheduler();
    for(;;);
    // never return
}