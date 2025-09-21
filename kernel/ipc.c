#include <common.h>
#include <memory.h>
#include <ipc.h>
#include <list.h>
#include <shmem.h>
#include <khash.h>

ipc_manager_t *gp_ipcm;

static khash_table_t   *mid_table;
static int ipc_id = 1;

static inline int ipc_alloc_mid()
{
    return __atomic_fetch_add(&ipc_id, 1, __ATOMIC_ACQ_REL);;
}

int ipc_init(void)
{
    gp_ipcm = (ipc_manager_t *)malloc(sizeof(ipc_manager_t));
    initlock(&gp_ipcm->lock, "ipcm_lock");
    list_init(&gp_ipcm->qlist);
    list_init(&gp_ipcm->shqlist);
        // 8 bits -> 256 buckets
    mid_table = khash_create(8);
    if (mid_table == NULL)
        panic("[IPC] can not create hash table");
    shmem_init();
    return 0;
}

uint64_t ipc_get_msg(uint64_t qkey, uint64_t flags)
{
    mqueue_t *q;
    //    debug("Get message queue qkey 0x%lX flags 0x%lX\n", qkey, flags);
    if (qkey == 0)
        return 0;

    acquire(&gp_ipcm->lock);
    list_for_each_entry(q, &gp_ipcm->qlist, qlist){
        if (q->qkey == qkey)
            goto found;
    }

    if (flags & IPC_EXIST){
        release(&gp_ipcm->lock);
        return 0;
    }

    q = (mqueue_t *)malloc(sizeof(mqueue_t));
    if (q == NULL)
        panic("IPC cannot alloc mem");

    initlock(&q->lock, "q lock");
//    list_init(&q->mlist);
    q->id    = ipc_alloc_mid();
    q->qkey  = qkey;
    q->flags = flags;
    q->task  = mytask();

    if (khash_insert(mid_table, q->id, q)){
        panic("[SHMEM] ins memblock to hash err");
        mfree(q);
        return 0;
    }
    list_init(&q->mlist);
    list_add(&gp_ipcm->qlist, &q->qlist);



found:
    release(&gp_ipcm->lock);
//        debug("Return qid 0x%lX\n", q->id);
    return (uint64_t)q->id;
}

/*
 * Send the IPC message
 * qid - queue ID
 * type - message type
 * ubuf - user space address of message
 * size - message size
 * flags - flags
 */
int ipc_snd_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags)
{
    mqueue_t *q;
    ipc_msg_t *msg;
    task_t *t;
    debug("Send msg qid 0x%lX type %d ubuf 0x%lX size %d\n", qid, type, ubuf, size);
    t = mytask();
    // should I check the task is valid ???????????

    if (qid == 0)
        return -ENOENT;
    q = khash_lookup(mid_table, qid);
    if (q == NULL)
        return -ENOENT;
    // ADD CHECKING RIGHTS TO ACCESS

    msg = malloc(sizeof(ipc_msg_t) + size);
    msg->type = type;
    list_init(&msg->mlist);
    if (size)
    {
        if (mmu_user_copyin(t->pagetable, (char *)&msg->message, ubuf, size) < 0)
        {
            mfree(msg);
            return -1;
        }
    }

    acquire(&q->lock);
    list_add_tail(&q->mlist, &msg->mlist);
    release(&q->lock);
    // wakeup waiters
    sched_task_wakeup(&q->mlist);
    return SUCCESS;
}

/*
 * Receive the IPC message
 * qid - queue ID
 * type - message type
 * ubuf - user space address of message
 * size - message size
 * flags - flags
 */
int ipc_rcv_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags)
{
    mqueue_t *q;
    int found;
    ipc_msg_t *msg;
    task_t *t;

    t = mytask();
    // should I check the task is valid ???????????
    debug("Task %d receive msg qid 0x%lX type %d ubuf 0x%lX size %d\n", t->pid, qid, type, ubuf, size);

    if (qid == 0)
        return -ENOENT;
    q = khash_lookup(mid_table, qid);
    if (q == NULL)
        return -ENOENT;
//    debug("[IPC] queue at 0x%lX\n", q);
    // ADD CHECKING RIGHTS TO ACCESS

    acquire(&q->lock);
    if (list_is_empty(&q->mlist)) {
        if (flags & IPC_NOWAIT)
            return 0;
        sched_task_sleep(&q->mlist, &q->lock);
    }

    // get next message
    found = 0;
    do {
        list_for_each_entry(msg, &q->mlist, mlist) {
            if (msg->type == type) {
                list_del(&msg->mlist);
                found = 1;
                break;
            }
        }
        if (found || flags & IPC_NOWAIT)
            break;
        sched_task_sleep(&q->mlist, &q->lock);
    } while (msg->type != type);

    release(&q->lock);
    debug("msg type %d\n", msg->type);
    if (found == 0 && flags & IPC_NOWAIT)
        return 0;
    if (size) {
        if (mmu_user_copyout(t->pagetable, ubuf, (char *)&msg->message, size) < 0) {
            /// return the message in the list
            acquire(&q->lock);
            list_add(&q->mlist, &msg->mlist);
            release(&q->lock);
            return -1; // return copy error
        }
    }
    mfree(msg);
    return 1;
}

/*
 * Create shared memory segment
 */
uint64_t ipc_get_shm(uint64_t key, size_t size, uint64_t flags)
{
    shmqueue_t *shm;
//        debug("Get message queue qkey 0x%lX flags 0x%lX\n", key, flags);
    if (key == 0)
        return 0;

    acquire(&gp_ipcm->lock);
    list_for_each_entry(shm, &gp_ipcm->shqlist, shqlist){
        if (shm->key == key)
            goto found;        
    }

    if (flags & IPC_EXIST){
        release(&gp_ipcm->lock);
        return 0;
    }

    shm = shmem_create(key, size, flags);
    if (shm == NULL){
        release(&gp_ipcm->lock);
        return 0;
    }

    list_add(&gp_ipcm->shqlist, &shm->shqlist);

found:
    release(&gp_ipcm->lock);
    debug("Return qid 0x%lX - 0x%lX pages\n",shm->id, shm->memblock->npages);
    return shm->id;
}


void *ipc_att_shm(uint64_t shmid, const void *addr, int flags)
{
    shmqueue_t *q;
    task_t      *t;
    pagetable_t pgtable;
    uint64_t    va, sz;
    shmem_page_t *p;
    debug("[IPC] shmat id 0x%lX addr 0x%lX fl 0x%lX\n", shmid, addr, flags);
    if (shmid == 0)
        return NULL;

    t = mytask();
    q = shmem_lookup(shmid);
    if (q == NULL)
        return NULL;

    sz = q->memblock->npages << PAGE_SHIFT;

    mem_reg_t *mreg = uvm_alloc_vmem(t, (uint64_t)addr, sz);
    if (mreg == NULL){
        panic("[IPC] can not allocate memreg");
        return NULL;      
    }

    mreg->shmem_block = q->memblock;
    pgtable = t->mm->pagetable;

    va = mreg->addr;
    p  = q->memblock->head;
    for (uint64_t a = va, i = 0; i < q->memblock->npages; i++, a += PAGE_SIZE){
        uint64_t pa = p->ppn << PAGE_SHIFT;
        if (p == NULL){
            panic("[IPC] shmem attach not full memblock");
            break;
        }        
        debug("Mapping va 0x%lX pa 0x%lX\n", a, pa);
        if (mmu_map_pages(pgtable, a, PAGE_SIZE, pa, PTE_R | PTE_U | PTE_W) != SUCCESS)
            return NULL; 
        p = p->next;    

        
    }

    return (void *)va;
}