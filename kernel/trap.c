#include <common.h>
#include <riscv.h>
#include <sched.h>
#include <trap.h>
#include <mmu.h>
#include <irq.h>
#include <signals.h>

extern uint64_t boot_hartid;

struct spinlock tickslock;
uint64_t ticks;

void
syscall(void);

// in kernelvec.S, calls kerneltrap().
void kernelvec();

void trap_init(void)
{
    initlock(&tickslock, "time");
    w_stvec((uint64_t)kernelvec);
}

/*
 *  Show task regs
 */

static void __show_regs(task_t *t)
{
    printf("Task registers:\n");
    printf("epc : 0x%016lX ra : 0x%016lX sp : 0x%016lX\n",
           t->trapframe->epc, t->trapframe->ra, t->trapframe->sp);
    printf(" gp : 0x%016lX tp : 0x%016lX t0 : 0x%016lX\n",
           t->trapframe->gp, t->trapframe->tp, t->trapframe->t0);
    printf(" t1 : 0x%016lX t2 : 0x%016lX s0 : 0x%016lX\n",
           t->trapframe->t1, t->trapframe->t2, t->trapframe->s0);
    printf(" s1 : 0x%016lX a0 : 0x%016lX a1 : 0x%016lX\n",
           t->trapframe->s1, t->trapframe->a0, t->trapframe->a1);
    printf(" a2 : 0x%016lX a3 : 0x%016lX a4 : 0x%016lX\n",
           t->trapframe->a2, t->trapframe->a3, t->trapframe->a4);
    printf(" a5 : 0x%016lX a6 : 0x%016lX a7 : 0x%016lX\n",
           t->trapframe->a5, t->trapframe->a6, t->trapframe->a7);
    printf(" s2 : 0x%016lX s3 : 0x%016lX s4 : 0x%016lX\n",
           t->trapframe->s2, t->trapframe->s3, t->trapframe->s4);
    printf(" s5 : 0x%016lX s6 : 0x%016lX s7 : 0x%016lX\n",
           t->trapframe->s5, t->trapframe->s6, t->trapframe->s7);
    printf(" s8 : 0x%016lX s9 : 0x%016lX s10: 0x%016lX\n",
           t->trapframe->s8, t->trapframe->s9, t->trapframe->s10);
    printf(" s11: 0x%016lX t3 : 0x%016lX t4 : 0x%016lX\n",
           t->trapframe->s11, t->trapframe->t3, t->trapframe->t4);
    printf(" t5 : 0x%016lX t6 : 0x%016lX\n",
           t->trapframe->t5, t->trapframe->t6);
}


void clockintr()
{
    acquire(&tickslock);
    ticks++;
//    wakeup(&ticks);
    release(&tickslock);
    heartbeat();
//    debug(".");
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr(void)
{
    uint64_t scause = r_scause();

    if ((scause & 0x8000000000000000L) &&
        (scause & 0xff) == 9)
    {
        // this is a supervisor external interrupt, via PLIC.

        // irq indicates which device interrupted.
        int irq = plic_claim();
        plic_irq_disable(current_cpu->hartid, irq);
//        debug("IRQ %d\n", irq);
        if (irq_send_signal(irq) != 0){
            printf("unexpected interrupt irq=%d\n", irq);
        }

        // the PLIC allows each device to raise at most one
        // interrupt at a time; tell the PLIC the device is
        // now allowed to interrupt again.
//        if (irq) {
//            plic_irq_enable(current_cpu->hartid, irq);
//            plic_complete(irq);
//        }

        return 1;
    }
    else if (scause == 0x8000000000000005L)
    {
        // Timer interrupt from CLINT timer interrupt

        if (current_cpu->hartid == boot_hartid)
        {
            clockintr();
        }

        // acknowledge the timer interrupt by setting next event
        sbi_set_timer(r_time() + usec_to_tick(TIMER_INTERVAL));
        return 2;
    }
    else
    {
        return 0;
    }
}



// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{

    int which_dev = 0;
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    uint64_t scause = r_scause();
    //  printf("trap in TP 0x%lX SSTATUS 0x%lX sepc 0x%lX\n", r_tp(), r_sstatus(), r_sepc());

    if ((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    if (intr_get() != 0)
            panic("kerneltrap: interrupts enabled");

    if ((which_dev = devintr()) == 0)
    {
        printf("HART %d task %d trap: scause %p sepc=%p stval=%p\n", current_cpu->hartid, mytask()->pid,
                scause, r_sepc(), r_stval());
        panic("kerneltrap");
    }
/*
    // give up the CPU if this is a timer interrupt.
    if (which_dev == 2 && mytask() != 0 && mytask()->state == RUNNING)
        yield();

    //  printf("0x%lX ", r_sip());
    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
*/
    w_sepc(sepc);
    w_sstatus(sstatus);

    //  printf("trap out TP 0x%lX SSTATUS 0x%lX sepc 0x%lX\n", r_tp(), r_sstatus(), r_sepc());
}

/*
 * handle an interrupt, exception, or system call from user space.
 * called from trampoline.S
*/
void usertrap(void)
{

    int which_dev = 0;
    
    if ((r_sstatus() & SSTATUS_SPP) != 0)
        panic("usertrap: not from user mode");

    // send interrupts and exceptions to kerneltrap(),
    // since we're now in the kernel.
    w_stvec((uint64_t)kernelvec);

    task_t *t = mytask();

    // save user program counter.
    t->trapframe->epc = r_sepc();

    if (r_scause() == 8)
    {
        // system call
        if (sched_get_task_killed(t))
            sched_task_exit(-1);
        //    printf("[SCHED] syscall from userspace\n");
        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        t->trapframe->epc += 4;

        // an interrupt will change sepc, scause, and sstatus,
        // so enable only now that we're done with those registers.
        intr_on();
//    debug("[TRAP] task SYSCALL required\n");
        syscall();
    }
    else if ((which_dev = devintr()) != 0)
    {
        // ok
    }
    else
    {
        printf("usertrap(): unexpected scause %p pid=%d hartid=%d\n", r_scause(), t->pid, current_cpu->hartid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        __show_regs(t);
        sched_set_task_killed(t);
    }

    if (sched_get_task_killed(t))
            sched_task_exit(-1);

        
    if (which_dev == 2){
        // give up the CPU if this is a timer interrupt.
        sched_task_yield();
    }
  
    usertrapret();
}

/*
 * return to user space
*/
void usertrapret(void)
{
    task_t *t = mytask();


    // check pending signals
    if (t->sighand != NULL){
        if (__atomic_load_n(&t->sighand->pending_mask, __ATOMIC_RELAXED)) {
//            debug("PENDING SIGNAL\n");
            signal_entry_t e;
            if (signal_getnext(t, &e) == SUCCESS) {
//            debug("Got sig %d payload %d\n", e.signal, e.payload);
                delivery_signal(t, e.signal, e.payload);
            }
        }
    } 

    // we're about to switch the destination of traps from
    // kerneltrap() to usertrap(), so turn off interrupts until
    // we're back in user space, where usertrap() is correct.
    intr_off();

    // if (mytask()->pid == 7) {
    //     printf("usertrap(): scause %p pid=%d hartid=%d\n", r_scause(), mytask()->pid, current_cpu->hartid);
    //     __show_regs(mytask());
    //     panic("break");
    // }
    // send syscalls, interrupts, and exceptions to uservec in trampoline.S

    w_stvec(kernel_map.uservec);

    // set up trapframe values that uservec will need when
    // the process next traps into the kernel.
    t->trapframe->kernel_satp = r_satp();                           // kernel page table
    t->trapframe->kernel_sp = t->kstack->start + t->kstack->size; // process's kernel stack   t->kstack + PAGESIZE
    t->trapframe->kernel_trap = (uint64_t)usertrap;
    t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()
    // if (t->pid == 7) {
    //     printf("usertrap(): scause %p pid=%d hartid=%d\n", r_scause(), t->pid, current_cpu->hartid);
    //     printf("    kernel stack %p pagetable %p\n", t->trapframe->kernel_sp, t->pagetable);
    //     __show_regs(t);
    //     mmu_pt_dump(t->pagetable);
    //     panic("break");
    // }
    // set up the registers that trampoline.S's sret will use
    // to get to user space.

    // set S Previous Privilege mode to User.
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);

    // set S Exception Program Counter to the saved user pc.
    w_sepc(t->trapframe->epc);

    // tell trampoline.S the user page table to switch to.
//    uint64_t satp = MAKE_SATP(DA2PA(t->pagetable));
    uint64_t satp = MAKE_USER_SATP(DA2PA(t->pagetable), t->asid);

    // debug("[TRAP] user trap return SATP 0x%lX\n", satp);
    // debug("[TRAP] user return addr SEPC 0x%lX\n", r_sepc());
    // debug("[TRAP] task %d return addr SEPC 0x%lX\n", t->pid, t->trapframe->epc);

    // jump to userret in trampoline.S at the top of memory, which
    // switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    // ??? Maybe calculating is more secure then keep it memory
//    uint64_t trampoline_userret = kernel_map.userret;
    uint64_t trampoline_userret = TRAMPOLINE + ((uint64_t)userret - (uint64_t)trampoline);

//    debug("[TRAP] trapoline user return address 0x%lX\n", trampoline_userret);

    ((void (*)(uint64_t, uint16_t))trampoline_userret)(satp, t->asid);
}

/*
 * handle an signal handle return from user space.
 * called from trampoline.S
*/
uint64_t usersigret(void)
{
    
    // if ((r_sstatus() & SSTATUS_SPP) != 0)
    //     panic("usertrap: not from user mode");

    // // send interrupts and exceptions to kerneltrap(),
    // // since we're now in the kernel.
    // w_stvec((uint64_t)kernelvec);

    task_t *t = mytask();


    trapframe_t *regs = t->trapframe;
    uint64_t    usp = t->trapframe->sp;

    mmu_user_copyin(t->pagetable, (char *)regs, usp, sizeof(trapframe_t));
//    intr_on();

//    w_sepc(t->trapframe->epc);
    // debug("Restored: \n");
    // __show_regs(t);

    return 0;
}

/*
 *  Call signal handler in user space for a task
 */
void delivery_signal(task_t * t, signal_t sig, signal_payload_t payload)
{
    trapframe_t *regs = t->trapframe;
    signal_hand_t *th = t->sighand;
//     debug("[SIGIN] task %d regs at 0x%lX th 0x%lX\n", t->pid, regs, th);
//  debug("[SIGIN] Delivering signal %d to task %d payload %d\n", sig, t->pid, payload);
    // __show_regs(t);
   
//    debug("Kernel SP 0x%lX EPC 0x%lX\n", r_sp(), r_sepc());

    // we're about to switch the destination of traps from
    // kerneltrap() to usertrap(), so turn off interrupts until
    // we're back in user space, where usertrap() is correct.
    intr_off();
    // send syscalls, interrupts, and exceptions to uservec in trampoline.S
    w_stvec(kernel_map.uservec);
    // set up trapframe values that uservec will need when
    // the process next traps into the kernel.
   t->trapframe->kernel_satp = r_satp();                           // kernel page table
   t->trapframe->kernel_sp = t->kstack->start + t->kstack->size; // process's kernel stack   t->kstack + PAGESIZE
   t->trapframe->kernel_trap = (uint64_t)usertrap;
   t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

    // set up the registers that trampoline.S's sret will use
    // to get to user space.

    // set S Previous Privilege mode to User.
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
//    x |= SSTATUS_SPIE; // no interrupts for signal handler
    w_sstatus(x);


    uint64_t usp = t->trapframe->sp - sizeof(trapframe_t);
    mmu_user_copyout(t->pagetable, usp, (char *)regs, sizeof(trapframe_t));
    t->trapframe->sp = usp;
    if (th->sa[sig].restorer != NULL)
        t->trapframe->ra = (uint64_t)th->sa[sig].restorer;
    t->trapframe->a0 = sig;
    t->trapframe->a1 = payload;

    acquire(&t->sighand->lock);
    if (t->sighand->sa[sig].handler == NULL)
        panic("NO HANDLER");
    
    // set S Exception Program Counter to Signal handler.
    w_sepc((uint64_t)t->sighand->sa[sig].handler);
    release(&t->sighand->lock);
    // tell trampoline.S the user page table to switch to.
    uint64_t satp = MAKE_USER_SATP(DA2PA(t->pagetable), t->asid);

//    debug("[TRAP] user trap return SATP 0x%lX\n", satp);
//    debug("[SIGIN] user return addr SEPC 0x%lX\n", r_sepc());
//    debug("[SIGIN] task %d return addr SEPC 0x%lX\n", t->pid, t->trapframe->epc);

    // jump to userret in trampoline.S at the top of memory, which
    // switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    // ??? Maybe calculating is more secure then keep it memory
//    uint64_t trampoline_userret = kernel_map.userret;
//    uint64_t trampoline_usersignal = TRAMPOLINE + ((uint64_t)usersignal - (uint64_t)trampoline);
    uint64_t trampoline_usersignal = TRAMPOLINE + ((uint64_t)userret - (uint64_t)trampoline);
//    debug("[TRAP] trapoline user return address 0x%lX\n", trampoline_userret);

    ((void (*)(uint64_t, uint16_t))trampoline_usersignal)(satp, t->asid);

}

