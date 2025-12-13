#include <common.h>
#include <riscv.h>
#include <sched.h>
#include <mmu.h>
#include <trap.h>
#include <khash.h>
#include <cap.h>
#include <sysproc.h>

struct cpu cpus[NCPUS];

// Task manager
task_manager_t g_taskmanager;
task_manager_t *gp_tm;

static khash_table_t *task_table;

// Nameserver's endpoint object
endpoint_t  *ns_ep;

/*
 * Scheduler initialization
 * Initialize the tasks table.
 */
void sched_init(void)
{
    gp_tm = &g_taskmanager;
    initlock(&gp_tm->pid_lock, "nextpid");
    initlock(&gp_tm->list_lock, "tasklist");
    initlock(&gp_tm->wait_lock, "wait_lock");

    gp_tm->nextpid = 1;
    list_init(&gp_tm->tasklist);

    task_table = khash_create(8);
    if (task_table == NULL)
        panic("[SCHED] can not create hash table");

//    debug("[SCHED] size of cap %d\n", sizeof(cap_entry_t));
}

/*
 * Return the current struct task *, or zero if none.
 */
task_t *
mytask(void)
{
    push_off();
    task_t *t = current_cpu->task;
    pop_off();
    return t;
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void sched_task_sleep(void *chan, struct spinlock *lk)
{
    task_t *t = mytask();

    // Must acquire t->lock in order to
    // change t->state and then call sched.
    // Once we hold t->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks t->lock),
    // so it's okay to release lk.

    acquire(&t->lock); // DOC: sleeplock1
    release(lk);

    // Go to sleep.
    t->chan = chan;
    set_task_state(t, SLEEPING);
//    debug("[SCHED] Task %d sleep\n", t->pid);
    sched_to_scheduler();

    // Tidy up.
    t->chan = 0;

    // Reacquire original lock.
    release(&t->lock);
    acquire(lk);
}

/*
 * 
 * 
 */
// void sched_sleep()
// {
//     task_t *t = mytask();

//     // Must acquire t->lock in order to
//     // change t->state and then call sched.
//     // Once we hold t->lock, we can be
//     // guaranteed that we won't miss any wakeup
//     // (wakeup locks t->lock),
//     // so it's okay to release lk.

//     acquire(&t->lock); // DOC: sleeplock1
// //    debug("task %d going SLEEP\n", t->pid);
//     // Go to sleep.
//     set_task_state(t, SLEEPING);
 
// //    debug("[SCHED] Task %d sleep\n", t->pid);
//     sched_to_scheduler();
//     release(&t->lock);
 
// }

void sched_task_block(void *chan, struct spinlock *lk, enum task_state state)
{
    task_t *t = mytask();

    // Must acquire t->lock in order to
    // change t->state and then call sched.
    // Once we hold t->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks t->lock),
    // so it's okay to release lk.
//    debug("\x1b[31m[SHCED]\x1b[0m task block %d\n", t->pid);
    acquire(&t->lock); 
    release(lk);

    t->chan = chan;
    set_task_state(t, state);

    sched_to_scheduler();

    t->chan = 0;
    release(&t->lock);
    acquire(lk);
}

void sched_task_unblock(void *chan)
{
    task_t *t;
    list_head_t *pos;

    list_for_each(pos, &gp_tm->tasklist)
    {
        t = list_entry(pos, task_t, tasklist);

        if (t != mytask())
        {
            acquire(&t->lock);
            uint32_t state = get_task_state(t);
            if (((state == BLOCKED_RECV) || 
                (state == BLOCKED_SEND) ||
                (state == BLOCKED_REPLY)) && t->chan == chan)
            {
                set_task_state(t, RUNNABLE);
//                debug("[SCHED] wake up task %d\n", t->pid);
            }
            release(&t->lock);
        }
    }
}

void sched_task_update_block(task_t *t, void *chan, enum task_state state)
{
    if (t != mytask())
    {
        acquire(&t->lock);
        t->chan = chan;
        set_task_state(t, state);
        release(&t->lock);
    }
}

/*
 * Wake up all processes sleeping on chan.
 * Must be called without any t->lock.
 */
void sched_task_wakeup(void *chan)
{
    task_t *t;
    list_head_t *pos;

    list_for_each(pos, &gp_tm->tasklist)
    {
        t = list_entry(pos, task_t, tasklist);

        if (t != mytask())
        {
            acquire(&t->lock);

            if (get_task_state(t) == SLEEPING && t->chan == chan)
            {
                set_task_state(t, RUNNABLE);
//                debug("[SCHED] wake up task %d\n", t->pid);
            }
            release(&t->lock);
        }
    }
}

/*
 * Wake up task sleeping.
 * Must be called without any t->lock.
 */
// void sched_wakeup(task_t *t)
// {
//     if (t != mytask()) {
//         acquire(&t->lock);

//         if (get_task_state(t) == SLEEPING) {
//             set_task_state(t, RUNNABLE);
// //            debug("[SCHED] wake up task %d\n", t->pid);
//         }
//         release(&t->lock);
//     }
// }

/*
 * Give up the CPU for one scheduling round.
 */
void sched_task_yield(void)
{
    task_t *t = mytask();
    acquire(&t->lock);
    set_task_state(t, RUNNABLE);
    sched_to_scheduler();
    release(&t->lock);
}

/*
 * Kill the process with the given pid.
 * The victim won't exit until it tries to return
 * to user space (see usertrap() in trap.c).
*/
int sched_task_kill(int pid)
{
    task_t *t;
    list_head_t *pos;

    list_for_each(pos, &gp_tm->tasklist)
    {
        t = list_entry(pos,task_t, tasklist);
        acquire(&t->lock);
        if(t->pid == pid){
            t->killed = 1;
            if(get_task_state(t) == SLEEPING){
                // Wake process from sleep().
                set_task_state(t, RUNNABLE);
            }
            release(&t->lock);
            return 0;
        }
        release(&t->lock);
    }
    return -1;
}

void sched_set_task_killed(task_t *t)
{
    acquire(&t->lock);
    t->killed = 1;
    release(&t->lock);
}

int sched_get_task_killed(task_t *t)
{
    int k;

    acquire(&t->lock);
    k = t->killed;
    release(&t->lock);
    return k;
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls sched_task_wait().
*/
void 
sched_task_exit(int status)
{

    struct task *t = mytask();

    if (t == gp_tm->inittask)
        panic("init exiting");
    /*
      // Close all open files.
      for(int fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
          struct file *f = p->ofile[fd];
          fileclose(f);
          p->ofile[fd] = 0;
        }
      }

      begin_op();
      iput(t->cwd);
      end_op();
      */
//    t->cwd = 0;

    acquire(&gp_tm->wait_lock);

    // Give any children to init.
//    reparent(t);

    // Parent might be sleeping in sched_task_wait().
    sched_task_wakeup(t->parent);

    acquire(&t->lock);

    t->xstate = status;
    set_task_state(t,ZOMBIE);

    release(&gp_tm->wait_lock);

    // Jump into the scheduler, never to return.
    sched_to_scheduler();
    panic("zombie exit");
}

/*
 * Switch to scheduler.  Must hold only t->lock
 * and have changed task->state. Saves and restores
 * intena because intena is a property of this
 * kernel thread, not this CPU. It should
 * be task->intena and task->noff, but that would
 * break in the few places where a lock is held but
 * there's no process.
*/
void sched_to_scheduler(void)
{
    int intena;
    task_t *t = mytask();

    if (!holding(&t->lock))
        panic("[SCHED] sched t->lock");
    if (current_cpu->noff != 1)
        panic("[SCHED] tasks locks");
    if (get_task_state(t) == RUNNING)
        panic("[SCHED] task running");
    if (intr_get())
        panic("[SCHED] task interruptible");

    intena = current_cpu->intena;
    swtch(&t->context, &current_cpu->context);
    current_cpu->intena = intena;
}

/*
 * Scheduler
 *
 */
void scheduler(void)
{
    task_t *t;
    struct cpu *c = current_cpu;

    list_head_t *pos;

    c->task = NULL;
    
    for (;;)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        sfence_vma();
//                debug("%d ", current_cpu->hartid);
        list_for_each(pos, &gp_tm->tasklist)
        {
            t = list_entry(pos, task_t, tasklist);
//            debug("[SCHED] check task 0x%lX\n", t);
//            debug("[SCHED] check task %d state %d\n", t->pid, t->state);
            acquire(&t->lock);
            if (get_task_state(t) == RUNNABLE)
            {
 //               debug("%d", t->pid);
//                 if (t->pid == 8) {
//                     debug(" %d ", current_cpu->hartid);
//  //                  panic(" task 7");
//                }
                // Switch to chosen process.  It is the process's job
                // to release its lock and then reacquire it
                // before jumping back to us.
                set_task_state(t, RUNNING);
                c->task = t;
                swtch(&c->context, &t->context);

                // Process is done running for now.
                // It should have changed its t->state before coming back.
                c->task = NULL;
            }

            release(&t->lock);
        }
        intr_on();
        __asm__ __volatile__("fence iorw, iorw\n"
                             "wfi\n"
                             : : : "memory");
    }
}

/*
 * Allocating next pid
 */
int sched_alloc_pid(void)
{
    int pid;

    acquire(&gp_tm->pid_lock);
    pid = gp_tm->nextpid++;
    release(&gp_tm->pid_lock);

    return pid;
}

/*
 * A fork child's very first scheduling by scheduler()
 * will swtch to forkret.
 */
void forkret(void)
{

    static int first = 1;

    // Still holding t->lock from scheduler.
    release(&mytask()->lock);
    
//    debug("[SCHED] TASK RELEASED\n");

    if (first)
    {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        //    fsinit(ROOTDEV);
    }
    usertrapret();
}

/*
 * Free task
 */
int sched_taskfree(task_t *t)
{
    if (t == NULL) {
        debug("[SCHED] %s zero pointer\n", __func__);
        return -1;
    }

    // Remove from hash
    khash_remove(task_table, t->pid);
    // Delete from list
    list_del(&t->tasklist);
    
    // ??? Add free of mm obj, endpoints

    // Free page tables
    if (t->trapframe)
        pgfree(DA2PPN(t->trapframe));
    if (t->pagetable)
        mmu_free_pagetable(t->pagetable);
//        sched_task_freepagetable(t->pagetable, t->sz);
    if (t->kstack)
        kstack_free(t->kstack);
    mfree(t);
    return 0;
}

/*
 *  Allocating a new task
 */
task_t *
sched_taskalloc(void)
{
    task_t       *t;
    mem_struct_t *mm;
    uint64_t     ppn;

    ppn = pgalloc();
    if (!ppn) return NULL;

    t = (task_t *)PPN2DA(ppn);
    memset(t, 0, sizeof(task_t));

    t = (task_t *)ko_init((kobject_t *)t, t, KO_TASK);

    initlock(&t->lock, "task");

//    debug("[SCHED] creating task 0x%lX size %d\n", t, sizeof(task_t));
    mm = malloc(sizeof(mem_struct_t));
    if (mm == 0)
        panic("[SHED] cannot alloc mem struct for task");

    memset(mm, 0, sizeof(mem_struct_t));
    t->mm = mm;

    mm->start_stack = USERSTACK;
    mm->stacktop    = mm->start_stack;
    mm->mmap_base   = USRMEMMAP_START;
    mm->mmap_addr   = mm->mmap_base;
    
    list_init(&mm->memlist);

    t->pid = sched_alloc_pid();
    set_task_state(t, NEW);

    // add to list
    list_add_tail(&gp_tm->tasklist, &t->tasklist);
    // insert ito hash
    khash_insert(task_table, t->pid, t);

    t->kstack = kstack_alloc();
    if (t->kstack == NULL)
        panic("Kstack allocating error due a task creating\n");

    // Allocate a trapframe page.
    ppn = pgalloc();
    if (!ppn) {
        sched_taskfree(t);
        return NULL;
    }
    t->trapframe = (trapframe_t *)PPN2DA(ppn);
    memset(t->trapframe, 0, PAGE_SIZE);

//    cap_insert(&t->caps[0], cap_create_node(t), CAP_CNODE, 0);
    int err = cap_init_cnode(t);
    if (err < 0) panic("init cnode");

    err = uvm_init_mnode(t);
    if (err < 0) panic("init mnode");

    // Allocate IPC buffer
    ppn = pgalloc();
    if (!ppn) {
        sched_taskfree(t);
        return NULL;
    }
    t->ipc_buf = (void *)PPN2DA(ppn);
    memset(t->ipc_buf, 0, PAGE_SIZE);

    int ns_cap;
    if (unlikely(t->pid == 1)) {
        // Create 1 task's endpoint for ns
        endpoint_t *ep = malloc(sizeof(endpoint_t));
        if (ep == NULL)
            panic("cannot create endpoint");
        
        ep = (endpoint_t *)ko_init((kobject_t *)ep, t, KO_ENDPOINT);
        ep->count = 0;
        initlock(&ep->lock, "endpoint");
        list_init(&ep->queue);
        ep->state = EP_STATE_IDLE;
        ep->owner = t;

        ns_cap = cap_install(t, ep, CAP_ENDPOINT, CRIGHT_SND | CRIGHT_RCV);
        ns_ep = ep;
    } else {
          // Add name server endpoint as cap 0x100
        ns_cap = cap_install(t, ns_ep, CAP_ENDPOINT, CRIGHT_SND);  
    }

//    debug("[SCHED] NS cap id 0x%lX\n", ns_cap);
    // Set up new context to start executing at forkret,
    // which returns to user space.

    t->context.ra = (uint64)forkret;
    t->context.sp = t->kstack->start + t->kstack->size;

    return t;
}

/*
 * Find task object by pid
 */
task_t *sched_find_task(uint64_t pid)
{
    return khash_lookup(task_table, pid);
}

/*
 * Create a user page table for a given process, with no user memory,
 *  but with trampoline and trapframe pages.
 */
pagetable_t
sched_task_pagetable(task_t *t)
{
    pagetable_t pgtable;

    // An empty page table.
    pgtable = mmu_user_pt_create();
    if (!pgtable) return NULL;

    t->asid = sched_alloc_asid(t->pid);

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    
    if (mmu_map_pages(pgtable, TRAMPOLINE, PAGE_SIZE, (uint64_t)trampoline - kernel_map.rel_offset, PTE_R | PTE_X) < 0)
    {
        mmu_user_pg_free(pgtable, 0);
        return 0;
    }

    // map the trapframe page just below the trampoline page, for
    // trampoline.S.
    if (mmu_map_pages(pgtable, TRAPFRAME, PAGE_SIZE, (uint64_t)DA2PA(t->trapframe), PTE_R | PTE_W) < 0)
    {
        mmu_user_unmap(pgtable, TRAMPOLINE, 1, 0);
        mmu_user_pg_free(pgtable, 0);
        return 0;
    }

    return pgtable;
}

/*
 * Free a process's page table, and free the
 * physical memory it refers to.
 */
void sched_task_freepagetable(pagetable_t pagetable, uint64_t sz)
{
    mmu_user_unmap(pagetable, TRAMPOLINE, 1, 0);
    mmu_user_unmap(pagetable, TRAPFRAME, 1, 0);
    mmu_user_pg_free(pagetable, sz);
}

/*
 * Allocating ASID for the task id.
 * Easy version for now.
 * Linux uses reusable CONTEXTID
 * !!!REWRITE
 */
uint16_t
sched_alloc_asid(uint64_t id)
{ 
    uint64_t asid = id;
    asid ^= asid >> 32;
    asid ^= asid >> 16;
    asid &= 0xFFFF;

    if(asid == 0) asid = 0xFFFF;
    // should be like (asid & kernel_map.asid_max)
    return 0;// (asid & ((1ULL << 16) - 1));
}


// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
sched_growtask(int n)
{
    uint64_t    sz;
    task_t *t = mytask();

 //   sz = t->sz;
    if (n > 0)
    {
        if ((sz = mmu_user_vmalloc(t->pagetable, sz, sz + n, PTE_W)) == 0)
        {
            return -1;
        }
    }
    else if (n < 0)
    {
        sz = mmu_user_vmdealloc(t->pagetable, sz, sz + n);
    }
 //   t->sz = sz;

    return 0;
}

int fl2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int sched_task_control(task_t *t)
{
    int op = syscall_get_MR(t, msgRegisters[0]);

    switch (op)
    {
    case TASK_OP_MEM_ALLOC: {
        int cap_id     = syscall_get_MR(t, 0);    
        uint64_t vaddr = syscall_get_MR(t, msgRegisters[1]);
        uint64_t size  = syscall_get_MR(t, msgRegisters[2]);
        uint64_t flags = syscall_get_MR(t, msgRegisters[3]);

        cap_entry_t *ce = cap_lookup(t, cap_id);
        if (!ce)  return -ERR_CAP_INVAL;

        if (ce->type != CAP_TASK) return -ENOPERM;

        task_t *task = (task_t *)ce->obj;

        
        if (mmu_memmap(task->pagetable, vaddr, size, PTE_R | PTE_U | PTE_W) != SUCCESS) {
            return -ENOMEM;
        }  
        break;
    }    
    
    case TASK_OP_MEM_MAP: {
        int cap_id     = syscall_get_MR(t, 0);
        uint64_t vaddr = syscall_get_MR(t, msgRegisters[1]);
        int mem_cap    = syscall_get_MR(t, msgRegisters[2]);
        uint64_t flags = syscall_get_MR(t, msgRegisters[3]);

        cap_entry_t *ce = cap_lookup(t, cap_id);
        if (!ce)  return -ERR_CAP_INVAL;

        if (ce->type != CAP_TASK) return -ENOPERM;

        task_t *task = (task_t *)ce->obj;

        ce = cap_lookup(t, mem_cap);
        if (!ce)  return -ERR_CAP_INVAL;

        if (ce->type != CAP_FRAME) return -ENOPERM; 
        
        vmem_block_t *vmem = (vmem_block_t *)ce->obj;

        mmu_move_pages(t->pagetable, task->pagetable, vmem->start, 
                    vaddr, vmem->size, PTE_U | PTE_R | fl2perm(flags));
        
 //       task->sz = vaddr + vmem->size;
        // free slot
        vmem->type  = VMEM_NONE;
        vmem->start = 0;
        vmem->size  = 0;

        cap_free(t, mem_cap);

        break;
    }

    case TASK_OP_RUN: {
        int cap_id = syscall_get_MR(t, 0);
        uint64_t entry_point = syscall_get_MR(t, msgRegisters[1]);

        cap_entry_t *ce = cap_lookup(t, cap_id);
        if (!ce)  return -ERR_CAP_INVAL;

        if (ce->type != CAP_TASK) return -ENOPERM;

        task_t *task = (task_t *)ce->obj;
    //    debug("\x1b[31m[TASK]\x1b[0m RUN  task %d at 0x%lX\n", task->pid, entry_point);
        acquire(&task->lock);
        task->trapframe->epc = entry_point;
        set_task_state(task, RUNNABLE);
        release(&task->lock);

        break;
    }

    default:
        break;
    }

    return SUCCESS;
}