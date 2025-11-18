#include <common.h>
#include <endpoint.h>
#include <cap.h>
#include <sched.h>
#include <errno.h>
#include <object.h>
#include <shmem.h>
#include <sysproc.h>


// void *create_cnode(void)
// {
//     void *cnode;
//     uint64_t ppn = pgalloc();
//     if (!ppn) return NULL;

//     cnode = (void *)PPN2DA(ppn);
//     memset(cnode, 0, PAGE_SIZE);

//     return cnode;
// }

int cap_install(struct task *t, void *obj, cap_type_t type, uint32_t rights)
{
    if (!t) return -EINVAL;
    acquire(&t->cap_lock);
    for (int i = 0; i < MAX_CAPS; i++) {
        if (t->caps[i].type == CAP_NONE) {
///            t->caps[i].valid = true; 
            t->caps[i].type = type;
            t->caps[i].obj = ko_get((kobject_t *)obj);
            t->caps[i].rights = rights;
//            debug("[CAP] created task %d cap id %d type %d rights %d\n", t->pid, i, t->caps[i].type, t->caps[i].rights);
            release(&t->cap_lock);
            return i; /* return cap id */
        }
    }
    release(&t->cap_lock);
    return -ENOSPC; /* table full */
}

cap_entry_t *cap_lookup(task_t *t, int cap_id)
{
    if (!t || cap_id < 0 || cap_id >= MAX_CAPS)
        return NULL;

    acquire(&t->cap_lock);        
    if (t->caps[cap_id].type == CAP_NONE){
        release(&t->cap_lock);
        return NULL;
    }
    cap_entry_t *e = &t->caps[cap_id];
    release(&t->cap_lock);

    return e;
}

void cap_destroy(kobject_t *ko)
{
    if (ko->type == KO_SHMEM) {
//        debug("[CAP] free shared memory 0x%lX\n", ko);
        shmem_free_memory((shmem_block_t *)ko);
    } else {
        mfree(ko);
    }
}

void cap_free(task_t *t, int cap_id)
{    
    if (!t || cap_id < 0 || cap_id >= MAX_CAPS) return; 
    acquire(&t->cap_lock);        
    
    if (t->caps[cap_id].type == CAP_NONE){
        release(&t->cap_lock);
        return;
    }    
    
    kobject_t *obj = (kobject_t *)t->caps[cap_id].obj;
    t->caps[cap_id].obj = NULL;    
///    t->caps[id].valid = false;
    t->caps[cap_id].type = CAP_NONE;        
    t->caps[cap_id].rights = 0;
    release(&t->cap_lock);
//    debug("[CAP] free kernel object refcount %d\n", obj->refcount);
    if (obj) ko_put(obj, cap_destroy);
}

task_t *cap_to_task(task_t *task, int cap_id)
{
    if (!task || cap_id < 0 || cap_id >= MAX_CAPS) return NULL; 

    cap_entry_t *ce = cap_lookup(task, cap_id);
    if (!ce) return NULL;

    kobject_t *ko = ce->obj;
    if (!ko) return NULL;

    return ko->owner;
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
        ko_put((kobject_t *)r, NULL);
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
//    initlock(&ep->lock, "endpoint");
//    list_init(&ep->msglist);
    list_init(&ep->queue);
//    ep->owner = t;
    ep->count = 0;
    ep->state = EP_STATE_IDLE;

    int ret = cap_install(t, ep, CAP_ENDPOINT, rights);
    if (ret < 0)
        mfree(ep);
    return ret;
}

/*
 * Syscall create fastcall capability
 */

// int fastcall_create(task_t *t, uint32_t rights)
// {
//     fastcall_t *fc = malloc(sizeof(fastcall_t));
//     if (fc == NULL) return -ENOMEM;

//     fc = (fastcall_t *)ko_init((kobject_t *)fc, t, KO_FASTCALL);
//     initlock(&fc->lock, "fastcall");
//     list_init(&fc->tlist);
//     fc->owner = t;

//     fc->count = 0;
//     int ret = cap_install(t, fc, CAP_FASTCALL, rights);
//     if (ret < 0)
//         mfree(fc);

//     return ret;
// } 

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
/*        
        case CAP_REPLAY:
            ret = replay_create(t, rights);
            break;
*/      
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
//    debug("[CAP] granting from task %d id %d to task %d id %d rights %d\n",
//            from->pid, from_cap_id, to->pid, to_slot, req_rights);
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

    if (to_slot >= 0) {
        if (to_slot >= MAX_CAPS) {
            new_cap = -EINVAL;
            goto out_unlock;
        }
        to_ce = &to->caps[to_slot];
        if (to_ce->type) {
            new_cap = -EEXIST;
        goto out_unlock;
    }
    i = to_slot;
    } else {
        // find free slot
        i = -1;
        for (int j = 0; j < MAX_CAPS; j++) {
            if (to->caps[j].type == CAP_NONE) { 
                i = j; 
                break; 
            }
        }

        if (i == -1) { 
            new_cap = -ENOSPC; 
            goto out_unlock; 
        }
        to_ce = &to->caps[i];
    }

    // Install capability into recipient's cap table 
    to_ce->type = from_ce->type;
    to_ce->obj = ko_get(obj);
    to_ce->rights = req_rights;

    
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
    
    return cap_grant_into(from, from_id, to, to_slot,req_rights);
}

/*
 * Transfer capability from task to task by known cap id.
 */
int sys_cap_transfer(int src_cap, int dest_cap, uint32_t req_rights)
{
//    debug("[CAP] transfer cap %d to %d\n", src_cap, dest_cap);
    task_t *src_task = mytask();
    task_t *dest_task = cap_to_task(src_task, dest_cap);
//    debug("[CAP] transfer from task %d to %d\n", src_task->pid, dest_task->pid);
    if (!dest_task || src_task == dest_task) return -EINVAL;
    return cap_grant_into(src_task, src_cap, dest_task, -1,req_rights);
}