#include <common.h>
#include <mmu.h>
#include <memory.h>
#include <sched.h>


kstack_manager_t g_kstack_manager;
kstack_manager_t *gp_ksm;

const struct kmem_cache_info 
kcache_idx_size[] = {
    {"kernel-32", 32},
    {"kernel-64", 64},
    {"kernel-128", 128},
    {"kernel-256", 256},
    {"kernel-512", 512},
    {"kernel-1024", 1024},
    {"kernel-2048", 2 * 1024},
    {"kernel-4096", 4 * 1024}
};

kmem_cache_t  kmem_cache[NELEM(kcache_idx_size)];


static int
__new_kmem_cache(int idx)
{

    kmem_cache_t *cache;
//    kmem_slub_t *slub;

    cache = &kmem_cache[idx];
//    debug("[KMEM] Creating kmem cache index %d name %s size 0x%x\n", idx,
//             kcache_idx_size[idx].name, kcache_idx_size[idx].size);
    cache->name = kcache_idx_size[idx].name;
    cache->size = kcache_idx_size[idx].size;
    initlock(&cache->lock, "kcache lock");
    list_init(&cache->qslub);   
    cache->slub = vm_new_slub(cache);
//    debug("[KMEM] slub 0x%lX nextfree addr 0x%lX nextfree 0x%lX\n", cache->slub, &cache->slub->nextfree, cache->slub->nextfree);
    cache->freelist = &cache->slub->nextfree;
//    debug("[KMEM] freelist addr 0x%lX freelist 0x%lX *freelist 0x%lX\n", &cache->freelist, cache->freelist, *cache->freelist);

//    list_add(&cache->qslub, &slub->slublist);
    cache->nslubs = 1;

    return 0;
}


kstack_t*
kstack_alloc(void)
{
    kstack_t *ks;

    acquire(&gp_ksm->lock);
    if(!list_is_empty(&gp_ksm->free)){
        ks = list_entry(gp_ksm->free.next, kstack_t, stacklist);
        list_del(&ks->stacklist);
        list_add(&gp_ksm->alloc, &ks->stacklist);
        release(&gp_ksm->lock);
        return ks;
    }
    release(&gp_ksm->lock);

    ks = (kstack_t *)malloc(sizeof(kstack_t));
    if(ks == NULL)
        return NULL;

    ks->start = vm_kstack_alloc(KSTACK_SIZE);
    if(ks->start == 0)
        return NULL;

    ks->size = KSTACK_SIZE;
    ks->stacklist.next = NULL;
    ks->stacklist.prev = NULL;

    acquire(&gp_ksm->lock);
    list_add(&gp_ksm->alloc, &ks->stacklist);
    release(&gp_ksm->lock);
 
    return ks;
}

int 
kstack_free(kstack_t *ks)
{
    memset((void *)ks->start, 0, ks->size);
    debug("[KSTACK] free stack 0x%lX start 0x%lX size 0x%lX\n", ks, ks->start, ks->size);
    acquire(&gp_ksm->lock);
    list_del(&ks->stacklist);
    list_add(&gp_ksm->free, &ks->stacklist);
    release(&gp_ksm->lock);
    return 0;
}

static void kmem_init_cma()
{
    gp_vmmgr->cma_mem_start = 0;
    for(int i=0; i < kernel_map.nmemblocks; i++){
        if (board_memmap[i].type == MEM_CMA) {
            if (gp_vmmgr->cma_mem_start) break;
            debug("[KMEM] Init CMA memory from 0x%lX to 0x%lX\n", board_memmap[i].base, board_memmap[i].top);
            gp_vmmgr->cma_mem_start = board_memmap[i].base;
            gp_vmmgr->cma_mem_end = board_memmap[i].top;
            gp_vmmgr->cma_mem_top = board_memmap[i].base;
        }
    }


}

uint64_t kmem_cma_alloc(size_t size)
{
    uint64_t cma_start;
    acquire(&gp_vmmgr->lock);
    if ((gp_vmmgr->cma_mem_top + size) > gp_vmmgr->cma_mem_end) {
        release(&gp_vmmgr->lock);
        return 0;
    }
    cma_start = gp_vmmgr->cma_mem_top;
    gp_vmmgr->cma_mem_top += size;
    release(&gp_vmmgr->lock);

    memset((void *)PA2DA(cma_start), 0, size);
    return cma_start;
}

static void kstack_init(void)
{
    gp_ksm = &g_kstack_manager;
    initlock(&gp_ksm->lock, "kstack");

    list_init(&gp_ksm->free);
    list_init(&gp_ksm->alloc);
}

void kmem_init(void)
{
    for(int i = 0; i < NELEM(kcache_idx_size); i++){
        __new_kmem_cache(i);
    } 
    kstack_init();
    kmem_init_cma();
}


void*
kmem_alloc(uint64_t size) 
{

    int idx;
    kmem_cache_t *cache;

    list_head_t *pos;
    kmem_slub_t *slub;
    uintptr_t *ret;


    if(size > PAGE_SIZE)
        panic("kmem_alloc size > 4096");

    idx = __kcache_index(size);
//    debug("[KMEM] Requested %d bytes cache index %d\n", size, idx);
    cache = &kmem_cache[idx];

    acquire(&cache->lock);

    if(*cache->freelist != NULL){
        ret = *cache->freelist;
        *cache->freelist = (void *)*ret;
        release(&cache->lock);
        return ret;
    }
    
    if(list_is_empty(&cache->qslub)){
        
        cache->slub = vm_new_slub(cache);
        debug("[KMEM] +++ allocating a new slub 0x%lX\n", cache->slub);
        cache->freelist = &cache->slub->nextfree;
        ret = *cache->freelist;
        *cache->freelist = (void *)*ret;
        release(&cache->lock);
        return ret;
    }

    debug("[KMEM] *** looking in slub queue\n");
    list_for_each(pos, &cache->qslub){
        slub = list_entry(pos, kmem_slub_t, slublist);
        if(slub->nextfree == NULL){
            panic("FULL SLUB IN LIST");
        }
        ret = slub->nextfree;
        slub->nextfree = (void *)*ret;
        if(slub->nextfree != NULL){
            cache->slub = slub;
            cache->freelist = &slub->nextfree;                     
        }
        list_del(&slub->slublist);
        release(&cache->lock);
        return ret;
    }
    release(&cache->lock);
    return 0;
}

void
kmem_free(void *ptr)
{

    int idx;
    kmem_slub_t *slub;
    uintptr_t *addr;

    idx = virt2sidx(ptr);
    slub = &slub_map[idx];

//    debug("[KMEM] slub nextfree 0x%lX\n",slub->nextfree);
    if(slub->slub_cache == NULL)
        goto not_slub;
//   debug("############ Free block 0x%lX  slub 0x%lX size %d\n", ptr, slub, slub->slub_cache->size);        
    acquire(&slub->slub_cache->lock);
    addr = (uintptr_t*)ptr;
    *addr = (uintptr_t)slub->nextfree;
    slub->nextfree = addr;
//    debug("[KMEM] slub nextfree 0x%lX\n",slub->nextfree);
    if(slub->slub_cache->slub != slub){
        // debug("[KMEM] slub 0x%lX slub_cache->slub 0x%lX slub->slub_cache->qslub 0x%lX 
        //         slub->slub_cache->qslub.next 0x%lX slub->slub_cache->qslub.prev 0x%lX\n",
        //         slub, slub->slub_cache->slub, slub->slub_cache->qslub, 
        //         slub->slub_cache->qslub.next, slub->slub_cache->qslub.prev);

        // debug("[KMEM] slab 0x%lX slub list next 0x%lX prev 0x%lX\n", 
        //         slub, slub->slublist.next, slub->slublist.prev);
        //    /// ??? not correct     
        if((slub->slublist.next == slub->slublist.prev)){
//            debug("!!!!!!!!!!!!!!!! add to list\n");

            list_add(&slub->slub_cache->qslub, &slub->slublist);
//            debug("[KMEM] slab 0x%lX slub list next 0x%lX prev 0x%lX\n", 
//                slub, slub->slublist.next, slub->slublist.prev);            
        }
    }

    release(&slub->slub_cache->lock);
    return;

not_slub:
    panic("kmem_free not slub");
}



void *malloc(uint64 size) {
    if(size < 0x20)
        size = 0x20;
    return kmem_alloc(size);
}

void mfree(void *ptr) {
    if(ptr)
        kmem_free(ptr);
}

/*
void kstack_test(void){
    kstack_t *s1, *s2, *s3;
    s1 = kstack_alloc();
    s2 = kstack_alloc();
    printf("[KSTACK] Allocated s1 0x%lX start 0x%lX size 0x%lX\n", s1, s1->start, s1->size);
    printf("[KSTACK] Allocated s2 0x%lX start 0x%lX size 0x%lX\n", s2, s2->start, s2->size);
    kstack_free(s1);
    printf("alloc stacks %d\n",list_count_nodes(&gp_ksm->alloc));
    printf("free stacks %d\n",list_count_nodes(&gp_ksm->free));
    s3 = kstack_alloc();
    printf("[KSTACK] Allocated s3 0x%lX start 0x%lX size 0x%lX\n", s3, s3->start, s3->size);
}
*/