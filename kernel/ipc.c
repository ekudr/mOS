#include <common.h>
#include <memory.h>
#include <ipc.h>
#include <list.h>
#include <shmem.h>
#include <khash.h>
#include <sysproc.h>
#include <notification.h>

const int msgRegisters[] = {
    2, 3, 4, 5, 6
};

// ipc_manager_t *gp_ipcm;

// static khash_table_t   *mid_table;
// static int ipc_id = 1;

// static inline int ipc_alloc_mid()
// {
//     return __atomic_fetch_add(&ipc_id, 1, __ATOMIC_ACQ_REL);;
// }


int copy_MRs(task_t *sender, uint64_t *sendBuf, task_t *receiver,
               uint64_t *recvBuf, uint64_t n)
{
    uint64_t i;

    /* Copy inline words */
    for (i = 0; i < n && i < IPC_MAX_REGS; i++) {
        syscall_set_MR(receiver, msgRegisters[i],
                    syscall_get_MR(sender, msgRegisters[i]));
    }

    if (!recvBuf || !sendBuf) {
        return i;
    }
    
    /* Copy out-of-line words */
    for (; i < n; i++) {
        recvBuf[i + 1] = sendBuf[i + 1];        
    }

    return i;
}

static inline uint64_t *get_extra_caps_pointer(task_t * t)
{
    uint64_t *caps = (uint64_t *)t->ipc_buf + IPC_MAX_MSG_LEN + 2;
    return caps;
}

static uint8_t ipc_caps_transfer(uint64_t info, task_t *sender, task_t *receiver)
{
    int i;
    int n = xcaps_from_msginfo_word(info);
    // debug("[CAP] ipc cap transfer %d caps from task %d to %d\n", xcaps_from_msginfo_word(info),
    //         sender->pid, receiver->pid);
//    ipc_msg_info_t mi = msginfo_from_word(info);
    
    uint64_t *sender_caps = get_extra_caps_pointer(sender);
//    debug("\x1b[31m[IPC]\x1b[0m ipc_buf 0x%lX caps 0x%lX\n", sender->ipc_buf, sender_caps);
    uint64_t *receiver_caps = get_extra_caps_pointer(receiver);

    for (i = 0; i < IPC_MAX_CAPS && sender_caps[i]!= 0 && i < n; i++) {
        // ??? check what rights should be provide
        receiver_caps[i] = cap_grant_into(sender, sender_caps[i], receiver, -1, 0, 0);
    }


//    mi.extra_caps = i;
    
//    info = word_from_msginfo(&mi);
    return i;
}

void do_ipc_transfer(task_t *sender, task_t *receiver)
{
    int msg_transferred;
    uint8_t xcaps;
    uint64_t info = sender->trapframe->a1;
    ipc_msg_info_t mi = msginfo_from_word(info);
    
    msg_transferred = copy_MRs(sender, (uint64_t *)sender->ipc_buf,
                                receiver, (uint64_t *)receiver->ipc_buf, mi.length);
      
    // Transfer extra caps

    if (xcaps_from_msginfo_word(info))
    {
       xcaps = ipc_caps_transfer(info, sender, receiver);
    }
  
    // update message info on transferred msg and caps

    mi.length = msg_transferred;
    mi.extra_caps = xcaps;

    info = word_from_msginfo(&mi);
    syscall_set_MR(receiver, 1, info);
    syscall_set_MR(receiver, 0, sender->pid);
}

int ipc_init(void)
{
    // gp_ipcm = (ipc_manager_t *)malloc(sizeof(ipc_manager_t));
    // initlock(&gp_ipcm->lock, "ipcm_lock");
    // list_init(&gp_ipcm->qlist);
    // list_init(&gp_ipcm->shqlist);
    //     // 8 bits -> 256 buckets
    // mid_table = khash_create(8);
    // if (mid_table == NULL)
    //     panic("[IPC] can not create hash table");
    // shmem_init();

    return 0;
}





/*
 *  Syscall IPC send
 *  t - task sender
 *  cap_id - capability id of sender task
 *  return status
 */
int sys_ipc_send(task_t *t, int cap_id, bool is_blocking, bool is_call)
{
    cap_entry_t *ce = cap_lookup(t, cap_id);
    if (!ce)  return -ERR_CAP_INVAL;

    if (ce->type != CAP_ENDPOINT || !(ce->rights & CRIGHT_SND))
        return -ENOPERM;

    endpoint_t *ep = (endpoint_t *)ce->obj;

    acquire(&ep->lock);
//    debug("\x1b[31m[IPC]\x1b[0m send task %d ep stat %d\n", t->pid, ep->state);
    switch (ep->state)
    {
    case EP_STATE_IDLE:
    case EP_STATE_SEND:
        if (is_blocking) {
        //    enqueue(t);
            set_task_state_docall(t, is_call);
            list_add_tail(&ep->queue, &t->eplist);
            ep->state = EP_STATE_SEND;
            sched_task_block(ep, &ep->lock, BLOCKED_SEND);
    //        sched_task_sleep(t, KO_LOCK(ep));
        }
        break;
    
    case EP_STATE_RECV:
#ifdef __DEBUG__
        if (list_is_empty(&ep->queue)) panic("endpoint queue empty");
#endif
// debug("\x1b[31m[IPC]\x1b[0m send task %d\n", t->pid);     
        task_t *receiver = list_first_entry(&ep->queue, struct task, eplist);
        list_del(&receiver->eplist);

        if (list_is_empty(&ep->queue)) ep->state = EP_STATE_IDLE;
        do_ipc_transfer(t, receiver);

        task_t *server = ep->owner;
        if (is_call) {
            
            int rpl_cap = cap_replay_install(server, t);
            if (rpl_cap < 0) return rpl_cap;

            server->reply_cap = rpl_cap;
            
        }
//        sched_task_wakeup(receiver);
        sched_task_unblock(ep);

        if (is_call) sched_task_block(t, &ep->lock, BLOCKED_SEND);
        break;
    }
    release(&ep->lock);


    return SUCCESS;
}



/*
 *  Syscall IPC receive
 *  t - task receiver
 *  cap_id - capability id of sender task
 *  info - message info word
 *  is_blocking - blocking/non-blocking receive
 *  returns message info
 */
int sys_ipc_recieve(task_t *t, int cap_id, bool is_blocking)
{
    task_t *sender;

    cap_entry_t *ce = cap_lookup(t, cap_id);

    if (!ce) return -ERR_CAP_INVAL;

    // CAP_NOTIFICATION: polymorphic dispatch to notification_wait
    if (ce->type == CAP_NOTIFICATION) {
        if (!(ce->rights & CRIGHT_RCV)) return -EPERM;
        return notification_wait(t, (notification_t *)ce->obj, is_blocking);
    }

    if (ce->type != CAP_ENDPOINT || !(ce->rights & CRIGHT_RCV)){
        return -EPERM;
    }

    // Fast-path: drain bound notification before touching endpoint
    if (t->bound_notif != NULL) {
        notification_t *notif = t->bound_notif;
        acquire(&notif->lock);
        uint64_t w = __atomic_exchange_n(&notif->word, 0, __ATOMIC_ACQ_REL);
        if (w != 0) {
            release(&notif->lock);
            syscall_set_MR(t, 0, w);
            syscall_set_MR(t, 1, msginfo_word_new(0, 0, 0, MSGINFO_NOTIFICATION));
            return SUCCESS;
        }
        release(&notif->lock);
    }

    endpoint_t *ep = (endpoint_t *)ce->obj;

    acquire(&ep->lock);
//    debug("\x1b[31m[IPC]\x1b[0m receive task %d\n", t->pid);
    switch (ep->state)
    {
    case EP_STATE_IDLE:
    case EP_STATE_RECV:

        if (is_blocking) {
            list_add_tail(&ep->queue, &t->eplist);
            ep->state = EP_STATE_RECV;
            sched_task_block(ep, &ep->lock, BLOCKED_RECV);
            // Woke up. Check whether a notification woke us instead of IPC.
            if (t->notif_word != 0) {
                uint64_t w = t->notif_word;
                t->notif_word = 0;
                // Remove from endpoint queue if not already dequeued by IPC sender
                if (t->eplist.next != NULL) {
                    list_del(&t->eplist);
                    if (list_is_empty(&ep->queue)) ep->state = EP_STATE_IDLE;
                }
                release(&ep->lock);
                syscall_set_MR(t, 0, w);
                syscall_set_MR(t, 1, msginfo_word_new(0, 0, 0, MSGINFO_NOTIFICATION));
                return SUCCESS;
            }
        } else {
            // set a0 = 0 (badge)
            syscall_set_MR(t, 0, 0);
        }
        break;
    
    case EP_STATE_SEND:
        
#ifdef __DEBUG__
        if (list_is_empty(&ep->queue)) panic("endpoint queue empty");
#endif
        // dequeue task
        sender = list_first_entry(&ep->queue, struct task, eplist);
        list_del(&sender->eplist);
//debug("\x1b[31m[IPC]\x1b[0m receive task %d\n", t->pid);
        if (list_is_empty(&ep->queue)) ep->state = EP_STATE_IDLE;
        do_ipc_transfer(sender, t);

        if (get_task_state_docall(sender)) {
            int rpl_cap = cap_replay_install(t, sender);
            if (rpl_cap < 0) return rpl_cap;

            t->reply_cap = rpl_cap;
            
            sched_task_update_block(sender, sender, BLOCKED_REPLY);

        } else {
            // check do i need to wake it up
            sched_task_unblock(ep);
        //    sched_task_wakeup(sender);            
        }



        break;
    }
    
    release(&ep->lock);

    return SUCCESS;
}


int sys_ipc_reply(task_t *t)
{
    cap_entry_t *ce = cap_lookup(t, t->reply_cap);
    if (!ce) return -ERR_CAP_INVAL;

    if (ce->type != CAP_REPLAY || !(ce->rights & CRIGHT_SND)){
        return -EPERM;
    }        

    replay_t *r = (replay_t *)ce->obj;
//    if (KO_TYPE(r) != KO_REPLAY) return -EINVAL; 

    task_t *client = r->sender;
    if (!client) return -EINVAL;
//    debug("\x1b[31m[IPC]\x1b[0m reply task %d to task %d\n", t->pid, client->pid);
    do_ipc_transfer(t, client);

//    debug("\x1b[31m[IPC]\x1b[0m client 0x%lX status task %d chan 0x%lX\n",client, client->pid, client->chan);
    // should be endpoit ???
    sched_task_unblock(client);

    // free reply cap
    cap_free(t, t->reply_cap);
    t->reply_cap = 0;

    return SUCCESS;
}

void *sys_ipc_shm_attach(task_t *t, int cap_id, const void *addr, int flags)
{
    pagetable_t pgtable;
    uint64_t    va, sz;
    shmem_page_t *p;
//    debug("[IPC] shmat id 0x%lX addr 0x%lX fl 0x%lX\n", shmid, addr, flags);
    if (cap_id == 0) return NULL;

    cap_entry_t *ce = cap_lookup(t, cap_id);
    if (!ce || ce->type != CAP_SHMEMORY || !(ce->rights & CRIGHT_MAP)){
        return NULL;
    }    

    shmem_block_t *shm = (shmem_block_t *)ce->obj;
//    if (shm->ko.type != KO_SHMEM) return NULL; 

    sz = shm->npages << PAGE_SHIFT;

    mem_reg_t *mreg = uvm_alloc_vmem(t, (uint64_t)addr, sz);
    if (mreg == NULL){
        panic("[IPC] can not allocate memreg");
        return NULL;      
    }

    mreg->shmem_block = shm;
    pgtable = t->pagetable;

    va = mreg->addr;
    p  = shm->head;
    for (uint64_t a = va, i = 0; i < shm->npages; i++, a += PAGE_SIZE){    
        if (p == NULL){
            panic("[IPC] shmem attach not full memblock");
            break;
        }    
        uint64_t pa = p->ppn << PAGE_SHIFT;   
//        debug("Mapping va 0x%lX pa 0x%lX\n", a, pa);
        if (mmu_map_pages(pgtable, a, PAGE_SIZE, pa, PTE_R | PTE_U | PTE_W) != SUCCESS)
            return NULL; 
        p = p->next;    
    }

    return (void *)va;
}