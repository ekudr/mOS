#ifndef __SCHED_H__
#define __SCHED_H__

#include <board.h>
#include <mmu.h>
#include <memory.h>
#include <list.h>
#include <signals.h>
#include <endpoint.h>
#include <cap.h>
#include <ipc.h>

#include <object.h>

register struct cpu *current_cpu __asm__("tp");

// Saved registers for kernel context switches.
typedef struct context
{
    uint64_t ra;
    uint64_t sp;

    // callee-saved
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
} context_t;

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
typedef struct trapframe
{
    /*   0 */ uint64 kernel_satp;   // kernel page table
    /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
    /*  16 */ uint64 kernel_trap;   // usertrap()
    /*  24 */ uint64 epc;           // saved user program counter
    /*  32 */ uint64 kernel_hartid; // saved kernel tp
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /*  56 */ uint64 gp;
    /*  64 */ uint64 tp;
    /*  72 */ uint64 t0;
    /*  80 */ uint64 t1;
    /*  88 */ uint64 t2;
    /*  96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
} trapframe, trapframe_t;

typedef struct fpu_state
{
    uint64 f0;
    uint64 f1;
    uint64 f2;
    uint64 f3;
    uint64 f4;
    uint64 f5;
    uint64 f6;
    uint64 f7;
    uint64 f8;
    uint64 f9;
    uint64 f10;
    uint64 f11;
    uint64 f12;
    uint64 f13;
    uint64 f14;
    uint64 f15;
    uint64 f16;
    uint64 f17;
    uint64 f18;
    uint64 f19;
    uint64 f20;
    uint64 f21;
    uint64 f22;
    uint64 f23;
    uint64 f24;
    uint64 f25;
    uint64 f26;
    uint64 f27;
    uint64 f28;
    uint64 f29;
    uint64 f30;
    uint64 f31;
    uint32 fcsr;
} fpu_state, fpu_state_t;





enum task_state
{
    NEW = 1,
    BLOCKED_RECV,
    BLOCKED_SEND,
    BLOCKED_REPLY,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};

struct kstack;
struct mem_struct;
struct mem_region;
struct ipc_msg;
/*
 * Per-process state
 */
typedef struct task
{
    struct kobject      hdr;
    struct spinlock     lock;

    // t->lock must be held when using these:
    uint64_t            state; // Process state
    void                *chan;              // If non-zero, sleeping on chan
    int                 killed;             // If non-zero, have been killed
    int                 xstate;             // Exit status to be returned to parent's wait
    int                 pid;                // Process ID

    // wait_lock must be held when using this:
    struct task         *parent;            // Parent process

    // these are private to the process, so p->lock need not be held.
    struct kstack       *kstack;            // Kernel stack for a task
    struct mem_struct   *mm;                // Memory structure
//    uint64_t            sz;                 // Size of process memory (bytes)
    //uint64_t            mmaddr;             // Memory map address
    pagetable_t         pagetable;          // User page table
    uint16_t            asid;               // ASID for SATP. Linux uses CONTEXTID
    struct trapframe    *trapframe;         // data page for trampoline.S
    struct context      context;            // swtch() here to run process
    fpu_state_t         *fpu_state;         // fpu context
    char                name[16];           // Process name (debugging)

    // Task list links
    list_head_t         tasklist;
//    uint64_t            irq_flag;
//    void                *irq_handler;
    list_head_t         eplist;             // link to endpoint list
    void                *ipc_buf;
    struct notification *bound_notif;       // bound notification (strong ref via ko_get)
    uint64_t             notif_word;        // notification word staged by notification_signal
    struct spinlock     cap_lock;
    cap_entry_t         caps[MAX_ROOT_CAPS];

//    struct spinlock     rep_lock;
    int                 reply_cap;
//    struct ipc_msg      *replay_msg;

//    struct task         *reply_to;  // make it as cap
    struct signal_hand   *sighand;

} task, task_t;

_Static_assert((sizeof(struct task) < 0x1000), "task_t structure size");

#define TASK_STATE_MASK (BIT(16)-1U)

#define TASK_STATE_DOCALL 16    // bit of state
#define TASK_STATE_DOCALL_MASK BIT(TASK_STATE_DOCALL)

#define get_task_state(task)     ((task->state)&TASK_STATE_MASK)
#define set_task_state(task, st) \
        (task->state = (task->state  & ~TASK_STATE_MASK)|(st & TASK_STATE_MASK))

#define get_task_state_docall(task) (((task->state) & TASK_STATE_DOCALL_MASK) >> TASK_STATE_DOCALL)
#define set_task_state_docall(task, v) \
        (task->state = (task->state  & ~TASK_STATE_DOCALL_MASK)|((v << TASK_STATE_DOCALL) & TASK_STATE_DOCALL_MASK))

// Per-CPU state.
struct cpu
{
    int hartid;
    task_t *task;          // The process running on this cpu, or null.
    context_t context; // swtch() here to enter scheduler().
    int noff;          // Depth of push_off() nesting.
    int intena;        // Were interrupts enabled before push_off()?
    struct kstack *stack;
};

typedef struct task_manager {
    int nextpid;
    struct spinlock pid_lock;
    struct spinlock list_lock;

    // helps ensure that wakeups of wait()ing
    // parents are not lost. helps obey the
    // memory model when using p->parent.
    // must be acquired before any p->lock.
    struct spinlock wait_lock;

    list_head_t tasklist;
    task_t *inittask;
} task_manager_t;

enum {
    TASK_OP_MEM_ALLOC = 1,
    TASK_OP_MEM_MAP,
    TASK_OP_RUN,

};

void sched_init(void);
task_t *mytask(void);
void scheduler(void);
void sched_to_scheduler(void);
void swtch(context_t*, context_t*);
int sched_task_kill(int pid);
void sched_set_task_killed(task_t *t);
int sched_get_task_killed(task_t *t);
void sched_task_yield(void);
void sched_task_wakeup(void *chan);
void sched_wakeup(task_t *t);
void sched_sleep();
void sched_task_sleep(void *chan, struct spinlock *lk);
void sched_task_block(void *chan, struct spinlock *lk, enum task_state state);
void sched_task_unblock(void *chan);
void sched_task_update_block(task_t *t, void *chan, enum task_state state);
void sched_task_exit(int status);
int sched_alloc_pid(void);
task_t *sched_find_task(uint64_t pid);
int sched_taskfree(task_t *t);
task_t *sched_taskalloc(void);
pagetable_t sched_task_pagetable(task_t *t);
void sched_task_freepagetable(pagetable_t pagetable, uint64_t sz);
kerrno_t uvm_alloc_mmreg(task_t *task, uint64_t vaddr, uint64_t size, uint64_t type, int xperm);
struct mem_region *uvm_alloc_vmem(task_t *task, uint64_t vaddr, size_t size);
struct mem_region *uvm_user_memmap(task_t *task, uint64_t addr, uint64_t size, uint64_t flags, uint64_t paddr);
int uvm_init_mnode(task_t *t);
vmem_block_t *uvm_find_free_slot(task_t *t);

int uvm_alloc_vm(task_t *task, uint64_t vaddr, size_t size, uint16_t type, int xperm);
int sched_task_control(task_t *t);

#endif /* __SCHED_H__ */