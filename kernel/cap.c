#include <common.h>
#include <endpoint.h>
#include <cap.h>
#include <sched.h>
#include <errno.h>
#include <object.h>


int cap_install(struct task *t, void *obj, cap_type_t type, uint32_t rights)
{
    acquire(&t->cap_lock);
    for (int i = 0; i < MAX_CAPS; i++) {
        if (!t->caps[i].valid) {
            t->caps[i].valid = true; 
            t->caps[i].type = type;
            t->caps[i].obj = obj;
            t->caps[i].rights = rights;
//            debug("[CAP] created task %d cap id %d type %d rights %d\n", t->pid, i, t->caps[i].type, t->caps[i].rights);
            release(&t->cap_lock);
            return i; /* return cap id */
        }
    }
    release(&t->cap_lock);
    return -ENOSPC; /* table full */
}

cap_entry_t *cap_lookup(task_t *t, uint32_t id)
{
    if (id < 0 || id >= MAX_CAPS)
        return NULL;
    if (t->caps[id].valid == false)
        return NULL;
    return &t->caps[id];
}

void cap_free(task_t *t, uint32_t id)
{
    if (id < 0 || id >= MAX_CAPS)
       return; 
    t->caps[id].valid = false;
    t->caps[id].obj = 0;
/*    
    t->caps[id].type = 0;        
    t->caps[id].rights = 0;
*/    
}

int sys_endpoint_create(task_t *t, uint32_t rights)
{    
    endpoint_t *ep = (endpoint_t *)ko_init(malloc(sizeof(endpoint_t)));
    if (ep == NULL)
        return -ENOMEM;
    
    ep->ko.type = KO_ENDPOINT;

    initlock(&ep->lock, "endpoint");
    list_init(&ep->msglist);
    ep->owner = t;

    ep->count = 0;
    int ret = cap_install(t, ep, CAP_ENDPOINT, rights);
    if (ret < 0)
        mfree(ep);
    return ret;
}
/*
int replay_create(task_t *t, uint32_t rights)
{
    endpoint_t *rp = (endpoint_t *)ko_init(malloc(sizeof(endpoint_t)));
    if (rp == NULL)
        return -ENOMEM;
    
    rp->ko.type = KO_REPLAY;

    initlock(&rp->lock, "replay");
    list_init(&rp->msglist);
    rp->owner = t;
//    debug("[CAP] Endpoint allocated 0x%lX\n", ep);
    rp->count = 0;
    int ret = cap_install(t, rp, CAP_REPLAY, rights);
    if (ret < 0)
        mfree(rp);
    return ret;
}
*/
/*
 * Syscall create fastcall capability
 */

int fastcall_create(task_t *t, uint32_t rights)
{
    fastcall_t *fc = (fastcall_t *)ko_init(malloc(sizeof(fastcall_t)));

    if (fc == NULL)
        return -ENOMEM;
    
    fc->ko.type = KO_FASTCALL;

    initlock(&fc->lock, "fastcall");
    list_init(&fc->tlist);
    fc->owner = t;

    fc->count = 0;
    int ret = cap_install(t, fc, CAP_FASTCALL, rights);
    if (ret < 0)
        mfree(fc);

    return ret;
} 

int sys_capability_create(task_t *t, cap_type_t type, uint32_t rights)
{
    int ret;
//    debug("[CAP] task %d create cap type %d\n", t->pid, type);
        switch (type)
        {
        case CAP_ENDPOINT:
            ret = sys_endpoint_create(t, rights);
            break;

        case CAP_FASTCALL:
            ret = fastcall_create(t, rights);
            break;
/*        
        case CAP_REPLAY:
            ret = replay_create(t, rights);
            break;
*/      
        default:
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
    if (!from_ce || !from_ce->valid) 
        return -EINVAL;

    // check 'from' has grant right on the cap
    if (!(from_ce->rights & CRIGHT_GRANT))
        return -ENOPERM;

    // requested_rights must be subset of from_ce->rights
    if ((req_rights & ~from_ce->rights) != 0)
        return -EINVAL;

    // Kernel object behind the cap
    obj = from_ce->obj;
    if (!obj) 
        return -EINVAL;

    // Lock both cap tables (use address compare to avoid deadlocks)
    if (from < to) {
        acquire(&from->cap_lock);
        acquire(&to->cap_lock);
    } else if (from > to) {
        acquire(&to->cap_lock);
        acquire(&from->cap_lock);
    } else {
        acquire(&from->cap_lock);
    }

    // If to_slot requested, verify free; else find free slot

    if (to_slot >= 0) {
        if (to_slot >= MAX_CAPS) {
            new_cap = -EINVAL;
            goto out_unlock;
        }
        to_ce = &to->caps[to_slot];
        if (to_ce->valid) {
            new_cap = -EEXIST;
        goto out_unlock;
    }
    i = to_slot;
    } else {
        // find free slot
        i = -1;
        for (int j = 0; j < MAX_CAPS; j++) {
            if (!to->caps[j].valid) { 
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
    to_ce->valid = true;
    to_ce->type = from_ce->type;
    to_ce->obj = ko_get(obj);
    to_ce->rights = req_rights;
    
    // bump kernel object refcount 
//    kernel_object_ref(obj);
    new_cap = i;

out_unlock:

    if (from < to) {
        release(&to->cap_lock);
        release(&from->cap_lock);
    } else if (from > to) {
        release(&from->cap_lock);
        release(&to->cap_lock);
    } else {
        release(&from->cap_lock);
    }
    return new_cap;
}

long sys_cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights)
{
    task_t *from = mytask();
    task_t *to = sched_find_task(to_pid);
    if (to == NULL)
        return -ENOENT;
    
        return cap_grant_into(from, from_id, to, to_slot,req_rights);
}