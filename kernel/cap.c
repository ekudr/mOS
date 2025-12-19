#include <common.h>
#include <endpoint.h>
#include <cap.h>
#include <sched.h>
#include <errno.h>
#include <object.h>
#include <shmem.h>
#include <sysproc.h>
#include <memory.h>



int cap_init_cnode(task_t *t)
{
    cap_node_t *cnode = cap_create_node(t);

    if (!cnode) return -ENOMEM;
    cap_insert(&t->caps[0], cnode, CAP_CNODE, 0);
    cap_insert(&cnode->caps[0], 0, CAP_NULL_CAP, 0);

    return SUCCESS;
}

cap_node_t *cap_create_node(task_t *t)
{
    cap_node_t *cnode;
    uint64_t ppn = pgalloc();
    if (!ppn) return NULL;

    cnode = (cap_node_t *)PPN2DA(ppn);
    memset(cnode, 0, PAGE_SIZE);

    cnode = (cap_node_t *)ko_init((kobject_t *)cnode, t, KO_CNODE);

    return cnode;
}

static cap_entry_t *__cap_lookup(task_t *t, int cap_id)
{
    int root = get_cap_root(cap_id);
    if (root > MAX_CAPS) return NULL;

    cap_entry_t *ce = &t->caps[root];
    if (unlikely(ce->type != CAP_CNODE)) return NULL;

    int idx = (cap_id >> 8) & 0xFF;
    if (idx > MAX_CAPS) return NULL;

    cap_node_t *cnode = (cap_node_t *)ce->obj;
    ce = &cnode->caps[idx];

    if (ce->type == CAP_CNODE) {
        idx = cap_id & 0xFF;
        if (idx > MAX_CAPS) return NULL;

        cap_node_t *cnode = (cap_node_t *)ce->obj;
        ce = &cnode->caps[idx];
    }

    return ce;
}

cap_entry_t *cap_lookup(task_t *t, int cap_id)
{
    acquire(&t->cap_lock);
    cap_entry_t *ce = __cap_lookup(t, cap_id);
    release(&t->cap_lock);
    return ce;
}

int cap_find_free_cap(task_t *t)
{
    if (!t) return -EINVAL;

    // cap root node
    cap_node_t *cnode = (cap_node_t *)t->caps[0].obj;


// ??? FIXME add CNODE search and addind ne CNODES

    for (int i = 0; i < MAX_CAPS; i++) {
        if (cnode->caps[i].type == CAP_NONE) {
            cnode->caps[i].type = CAP_UNTYPED;
            return i << 8; /* return cap id */
        }
    }
    return -ENOSPC; /* table full */
}

int cap_install(struct task *t, void *obj, cap_type_t type, uint32_t rights)
{
    if (!t || !obj || !type) return -EINVAL;
    acquire(&t->cap_lock);
    int cap_id = cap_find_free_cap(t);
    release(&t->cap_lock);
    if (cap_id < 0) return cap_id;

    cap_entry_t *ce = cap_lookup(t, cap_id);
    if (!ce) return -ENOSPC; // ??? need right err no

    cap_insert(ce, obj, type, rights);


    return cap_id;
}

int _cap_install(struct task *t, cap_entry_t *parent, void *obj, cap_type_t type, uint32_t rights)
{
    if (!t || !obj || !type) return -EINVAL;

    acquire(&t->cap_lock);
    int cap_id = cap_find_free_cap(t);
    release(&t->cap_lock);
    
    if (cap_id < 0) return cap_id;

    cap_entry_t *ce = cap_lookup(t, cap_id);
    if (!ce) return -ENOSPC; // ??? need right err no

    cap_insert(ce, obj, type, rights);


    return cap_id;
}

void cap_destroy(kobject_t *ko, uint32_t type)
{
    if (type == CAP_SHMEMORY) {
//        debug("[CAP] free shared memory 0x%lX\n", ko);
        shmem_free_memory((shmem_block_t *)ko);
    } else {
        mfree(ko);
    }
}

void cap_free(task_t *t, int cap_id)
{    
    if (!t) return; 

    cap_entry_t *ce = cap_lookup(t, cap_id);
    if (!ce) return;

    acquire(&t->cap_lock);        
    
    if (ce->type == CAP_NONE){
        release(&t->cap_lock);
        return;
    }    
    uint32_t type  = ce->type;
    kobject_t *obj = (kobject_t *)ce->obj;
    ce->obj        = NULL;    
    ce->type       = CAP_NONE;        
    ce->rights     = 0;
    release(&t->cap_lock);
//    debug("[CAP] free kernel object refcount %d\n", obj->refcount);

    if (obj) ko_put(obj, type, cap_destroy);
}


/*
 * Create transient reply cap i server's cap tabble.
 * Return cap_id
 */
int cap_replay_install(task_t *server, task_t *client)
{
    if (!server || !client) return -EINVAL;
    replay_t *r = malloc(sizeof(replay_t));
    if (!r) return -ENOMEM;

    r = (replay_t *)ko_init((kobject_t *)r, server, KO_REPLAY);
    r->sender = client;

    int cap_id = cap_install(server, r, CAP_REPLAY, CRIGHT_SND);
    if (cap_id < 0) {
        ko_put((kobject_t *)r, CAP_REPLAY, NULL);
        return -ENOSPC;
    }
    return cap_id;
}

int sys_endpoint_create(task_t *t)
{    
    uint32_t rights = syscall_get_MR(t, msgRegisters[1]);

    if (!rights) return -EINVAL;

    endpoint_t *ep = malloc(sizeof(endpoint_t));
    if (ep == NULL) return -ENOMEM;    

    ep = (endpoint_t *)ko_init((kobject_t *)ep, t, KO_ENDPOINT);
    initlock(&ep->lock, "endpoint");
//    list_init(&ep->msglist);
    list_init(&ep->queue);
    ep->owner = t;
    ep->count = 0;
    ep->state = EP_STATE_IDLE;

    int ret = cap_install(t, ep, CAP_ENDPOINT, rights);
    if (ret < 0)
        mfree(ep);
    return ret;
}

/*
 * Create shared memory capability
 * Return cap_id
 */
int sys_cap_shmem_create(task_t *t)
{
    int ret;
    uint32_t rights = syscall_get_MR(t, msgRegisters[1]);
    size_t size     = syscall_get_MR(t, msgRegisters[2]);

    if (!size || !rights) return -EINVAL;
//    debug("[CAP] Create shmem cap size 0x%lX\n", size);
    size_t sz = PGROUNDUP(size);

    shmem_block_t *shm = malloc(sizeof(shmem_block_t));
    if (shm == NULL) return -ENOMEM;

    shm = (shmem_block_t *)ko_init((kobject_t *)shm, t, KO_SHMEM);
      
    shm->size = size;
    shm->npages = sz >> PAGE_SHIFT;

    ret = shmem_alloc_memory(shm);
    if (ret < 0) {
        mfree(shm);
        return ret;
    }

    ret = cap_install(t, shm, CAP_SHMEMORY, rights);
    if (ret < 0) {
        // free_shmem()
        mfree(shm);
    }
        
    return ret;
}

int cap_task_create(task *t)
{
    int ret;
    task_t *new = sched_taskalloc();
    if (!new) return -ENOMEM;

    acquire(&new->lock);
    // An empty user page table.
    new->pagetable = sched_task_pagetable(new);

    if (new->pagetable == 0)
    {
        sched_taskfree(new);
        return -ENOMEM;
    }   


    if (uvm_alloc_mmreg(new, 0, 2 * PAGE_SIZE, MM_REG_STACK, PTE_W) < 0)
            panic("[LOADER] ERROR ALOCATING MEM_REG_STACK");
    new->trapframe->sp = new->mm->start_stack;   // initial stack pointer

    /// ??? rewrite memory mapping for allocated buf
    mem_reg_t * mreg = uvm_user_memmap(new, 0, PAGE_SIZE, MAP_MEMIO | MAP_READ | MAP_WRITE, DA2PA(t->ipc_buf));

    new->trapframe->a0 = mreg->addr;

    ret = cap_install(t, new, CAP_TASK, 0);
    if (ret < 0) {
        sched_taskfree(new);
    }

    release(&new->lock);

    return ret;
}

int cap_frame_create(task *t)
{
    int ret;

    uint32_t rights = syscall_get_MR(t, msgRegisters[1]);
    size_t size     = syscall_get_MR(t, msgRegisters[2]);
    uint64_t vaddr  = syscall_get_MR(t, msgRegisters[3]);

    if (!vaddr || !size) return -EINVAL;

    size  = PGROUNDUP(size);
    vaddr = PGROUNDDOWN(vaddr);
    

    // lock
    vmem_block_t *slot = uvm_find_free_slot(t);

    if (!slot) {
        //unlock
        return -ENOSPC;
    }

    ret = mmu_memmap(t->pagetable, vaddr, size, PTE_R | PTE_U | PTE_W);
    if (ret < 0) {
        // free slot
        return ret;
    }   

    slot->start = vaddr;
    slot->size = size >> PAGE_SHIFT;
    slot->type = VMEM_MEM;

    //unlock
    ret = cap_install(t, slot, CAP_FRAME, rights);
    if (ret < 0) {
        // free vmem slot 
        ;
    }
    
    return ret;
}    

int cap_dma_frame(task *t)
{
    int ret;
    uint32_t rights = syscall_get_MR(t, msgRegisters[1]);
    size_t size     = syscall_get_MR(t, msgRegisters[2]);

    // ??? FIXME allocating one page only now
    if (!size || size > PAGE_SIZE) return -EINVAL;

    dma_mem_block_t *mem = malloc(sizeof(dma_mem_block_t));
    if (!mem) return -ENOMEM;

    mem = (dma_mem_block_t *)ko_init((kobject_t *)mem, t, KO_SHMEM);

    uint64_t ppn = pgalloc();
    if (!ppn) return -ENOMEM;

    mem->addr = PPN2PA(ppn);
    mem->size = PAGE_SIZE;

    ret = cap_install(t, mem, CAP_DMA_FRAME, 0);
    if (ret < 0) {
        // free_shmem()
        mfree(mem);
    }

    syscall_set_MR(t, msgRegisters[1], mem->addr);

    return ret;
}

int sys_capability_create(task_t *t)
{
    int ret;
    cap_type_t type = (cap_type_t)syscall_get_MR(t, msgRegisters[0]);

//    debug("[CAP] task %d create cap type %d\n", t->pid, type);

    switch (type)
        {
        case CAP_ENDPOINT:
            ret = sys_endpoint_create(t);
            break;

        case CAP_SHMEMORY:           
            ret = sys_cap_shmem_create(t);
            break;
        
        case CAP_TASK:
            ret = cap_task_create(t);
            break;

        case CAP_FRAME:
            ret = cap_frame_create(t);
            break;

        case CAP_DMA_FRAME:
            ret = cap_dma_frame(t);
            break;

        default:
            ret = -EINVAL;
            break;
    }

    return ret;
} 

/*
* Grant a capability from 'from' (cap_id) into 'to' task.
* If to_slot == -1 then allocate a free slot in 'to'.
* requested_rights is the rights to give to 'to' (must be subset of original rights).
*
* Returns: new cap id in recipient (>=0) or negative errno.
*
* Caller must ensure tasks are valid. This function acquires locks on both
* tasks' cap tables in a consistent order to avoid deadlocks.
*/
int cap_grant_into(task_t *from, int from_cap_id,
                    task_t *to, int to_slot, uint32_t req_rights)
{
    cap_entry_t *from_ce, *to_ce;
    void *obj;
    int new_cap = -EINVAL;
    int i;
    // debug("[CAP] granting from task %d id %d to task %d id %d rights %d\n",
    //         from->pid, from_cap_id, to->pid, to_slot, req_rights);
    if (!from || !to) 
        return -ENOENT;

    // Lookup 'from' capability
    from_ce = cap_lookup(from, from_cap_id);
   
    if (!from_ce || (from_ce->type == CAP_NONE)) 
        return -EINVAL;

    // check 'from' has grant right on the cap
    if (!(from_ce->rights & CRIGHT_GRANT))
        return -ENOPERM;

    // copy original rights if req_rights == 0
    if (!req_rights) req_rights = from_ce->rights;
    // requested_rights must be subset of from_ce->rights
    if ((req_rights & ~from_ce->rights) != 0)
        return -EINVAL;

    // Kernel object behind the cap
    obj = from_ce->obj;
    if (!obj) 
        return -EINVAL;

    // Lock both cap tables (use address compare to avoid deadlocks)
    if (from == to) {
        acquire(&from->cap_lock);
    } else {
        if (from < to) {
            acquire(&from->cap_lock);
            acquire(&to->cap_lock);
        } else if (from > to) {
            acquire(&to->cap_lock);
            acquire(&from->cap_lock);
        } else {
            acquire(&from->cap_lock);
        }        
    }
    // If to_slot requested, verify free; else find free slot

    if (to_slot > 0) {

        to_ce = __cap_lookup(to, to_slot);  
//    debug("\x1b[31m[CAP]\x1b[0m to ce 0x%lX type %d\n", to_ce, to_ce->type);              
        if (!to_ce) {
            new_cap = -EINVAL;
            goto out_unlock;
        }

        if (to_ce->type != CAP_UNTYPED || to_ce->type != CAP_NONE) {
            new_cap = -EEXIST;
        goto out_unlock;
    }
    i = to_slot;
    } else {
        // find free slot
             
        i = cap_find_free_cap(to);
// debug("\x1b[31m[CAP]\x1b[0m found free 0x%lX\n", i);
        if (i < 0) { 
            new_cap = -ENOSPC; 
            goto out_unlock; 
        }
//        debug("\x1b[31m[CAP]\x1b[0m to 0x%lX 0x%lX\n", to, i);
        to_ce = __cap_lookup(to, i);
//        debug("\x1b[31m[CAP]\x1b[0m to_ce 0x%lX\n", to_ce);
    }

    // Install capability into recipient's cap table 
    cap_insert(to_ce, obj, from_ce->type, req_rights);
    
    new_cap = i;

out_unlock:

    if (from == to) {
        release(&to->cap_lock);
    } else {
        if (from < to) {
            release(&to->cap_lock);
            release(&from->cap_lock);
        } else if (from > to) {
            release(&from->cap_lock);
            release(&to->cap_lock);
        } else {
            release(&from->cap_lock);
        }
    }

    return new_cap;
}


long sys_cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights)
{
    task_t *from = mytask();
    task_t *to = sched_find_task(to_pid);
    if (!to) return -ENOENT;
//    debug("[CAP] transfer cap from %d 0x%lX to %d 0x%lX\n", from->pid, from_id, to_pid, to_slot);
    return cap_grant_into(from, from_id, to, to_slot,req_rights);
}

/*
 * Transfer capability from task to task by known cap id.
 */
// int sys_cap_transfer(int src_cap, int dest_cap, uint32_t req_rights)
// {
// //    debug("[CAP] transfer cap %d to %d\n", src_cap, dest_cap);
//     task_t *src_task = mytask();
//     task_t *dest_task = cap_to_task(src_task, dest_cap);
// //    debug("[CAP] transfer from task %d to %d\n", src_task->pid, dest_task->pid);
//     if (!dest_task || src_task == dest_task) return -EINVAL;
//     return cap_grant_into(src_task, src_cap, dest_task, -1,req_rights);
// }