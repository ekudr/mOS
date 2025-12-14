#include <common.h>
#include <mmu.h>
#include <memory.h>
#include <sched.h>
#include <riscv.h>
#include <trap.h>
#include <servers_loader.h>
#include <ipc.h>
#include <irq.h>
#include <fpu.h>

extern uint64_t boot_hartid;
extern struct cpu cpus[NCPUS];

extern uint64_t _imglen;

void plic_init(void);
void sbi_init (void);
int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv);
void _hart_start(void);

void heartbeat_init(void);

static void init_fpu(void)
{
    set_fs_clean();
    w_fcsr(0);

}


void
board_start_harts(void) {
    for (int i = 0; i < NCPUS; i++) {
        if(i != HARTID2CPU(current_cpu->hartid)) {
            sbi_hsm_hart_start(CPU2HARTID(i), (uint64)_hart_start - kernel_map.rel_offset, 2);
            udelay(100000);             
        }       
    }
}

void __attribute__((noreturn))
kernel_init(void)
{
    early_printf("Booting on HART %d\n", boot_hartid);
    // init per-CPU structures before vm_init
    // it needs for spinlock
#ifdef __SPACEMIT_K1__
    boot_hartid--;
#endif    
    current_cpu = &cpus[HARTID2CPU(boot_hartid)];
    current_cpu->hartid = boot_hartid;
    early_printf("Stack SP 0x%lX\n", r_sp());



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

    heartbeat_init();

    // disable FPU access 
    set_fs_off();

    init_fpu();


    ipc_init();
    sched_init();

    board_start_harts();

    load_servers();


    scheduler();
    for(;;);
    // never return
}

void __attribute__((noreturn))
boot_init_hart(int hartid)
{
    current_cpu = &cpus[HARTID2CPU(hartid)];
    current_cpu->hartid = hartid;
//    early_printf("Stack SP 0x%lX\n", r_sp());
//    mmu_switch_pagetable((uint64_t)kernel_pagetablet, 0);

    current_cpu->stack = kstack_alloc();
    // should be reserved stack for this function
    // compiler gives 32 bytes in this time
    // check in other situation
    // !!!REWRITE
    uint64_t sp = current_cpu->stack->start + current_cpu->stack->size - 0x10;
    __asm__ __volatile__("mv sp, %0" : : "r" (sp));

    printf("HART %d Stack SP 0x%lX\n", current_cpu->hartid, r_sp());    
    
    plic_cpu_enable(current_cpu->hartid);
    // enable the interrupts
    w_sstatus(r_sstatus() | SSTATUS_SIE);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    //Set TIMER
    sbi_set_timer(r_time() + usec_to_tick(TIMER_INTERVAL));

    // disable FPU access 
    set_fs_off();

    init_fpu();

    scheduler();
    for(;;);
    // never return
}