#include <common.h>
#include <mmu.h>

#include <memory.h>
//#include <ipc.h>
//#include <khash.h>

// static khash_table_t   *shmemid_table;
// static int shmem_block_id = 1;

// static inline int shmem_alloc_id()
// {
//     return __atomic_fetch_add(&shmem_block_id, 1, __ATOMIC_ACQ_REL);;
// }

static int __shmem_alloc_pages(shmem_block_t * shmb)
{
    uint64_t        ppn;
    shmem_page_t    *page, *prev_page = NULL;
//    debug("[SHMEM] creating 0x%lX pages\n", shmb->npages);
    for (int i = 0; i < shmb->npages; i++){
        ppn = pgalloc();
        if (ppn == 0){
            panic("[SHMEM] no memory");
//          unmap(shmb) 
            return -ENOMEM;
        }
        page = &ppn_to_page(ppn)->shmem_page;
        page->ppn = ppn;
        page->head = shmb;
        page->next = NULL;
        if (prev_page != NULL){
            prev_page->next = page;            
        } else {
            shmb->head = page;
        }
//        debug("page ppn 0x%lx\n", ppn);    
        prev_page = page;
    }

    return SUCCESS;
}

int shmem_alloc_memory(shmem_block_t * shmb)
{
    return __shmem_alloc_pages(shmb);
}


int shmem_free_memory(shmem_block_t * shmb)
{
    shmem_page_t    *page, *next;
    int pg, cnt;

    pg  = shmb->npages;
    cnt = 0;

    for (page = shmb->head; page; page=next, cnt++) {
        next = page->next;
        pgfree(page->ppn);
    }

    if (pg != cnt)
        panic("[SHMEM] wrong pages count");

    shmb->npages = 0;
    shmb->head = NULL;

    return SUCCESS;
}

// static shmem_block_t *shmem_create_memblock(size_t size)
// {
//     shmem_block_t   *shmb;
//     size_t          sz = PGROUNDUP(size);

//     shmb = malloc(sizeof(shmem_block_t));
//     if (shmb == NULL){
//         panic("[SHMEM] can to allocate mem");
//         return NULL;
//     }
        
    
//     shmb->size = size;
//     shmb->npages = sz >> PAGE_SHIFT;

//     if (__shmem_alloc_pages(shmb) != SUCCESS){
//         mfree(shmb);
//         return NULL;
//     }



//     return shmb;
// }

// shmqueue_t *shmem_create(uint64_t key, size_t size, uint64_t flags)
// {
//     shmqueue_t *shm;
//     shm = (shmqueue_t *)malloc(sizeof(shmqueue_t));
//     if (shm == NULL){
//         panic("IPC cannot alloc mem");
//         return 0;
//     }
        
//     initlock(&shm->lock, "shm lock");

//     shm->id = shmem_alloc_id();
//     shm->key = key;
//     shm->flags = flags;
//     shm->task = mytask();

//     shm->memblock = shmem_create_memblock(size);
//     if (shm->memblock == NULL){
//         mfree(shm);
//         return 0;
//     }

//     if (khash_insert(shmemid_table, shm->id, shm)){
//         panic("[SHMEM] ins memblock to hash err");
//         mfree(shm);
//         return NULL;
//     }
//     return shm;
// }

// shmqueue_t *shmem_lookup(uint64_t shmid)
// {
//     return khash_lookup(shmemid_table, shmid);
// }


// kerrno_t shmem_init(void)
// {
//     // 8 bits -> 256 buckets
//     shmemid_table = khash_create(8);
//     if (shmemid_table == NULL)
//         return -ENOMEM;
//     return SUCCESS;
// }