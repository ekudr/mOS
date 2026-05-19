#include <common.h>
#include <mmu.h>
#include <atomic.h>
#include <memory.h>


kmem_slub_t *slub_map;

vmem_mgr_t vm_mgr;
vmem_mgr_t *gp_vmmgr;


uintptr_t
vm_alloc(uint64_t size)
{
    uintptr_t start, va;
    uint64_t ppn;

    if ((size % PAGE_SIZE) != 0)
        panic("VMEM kstack size should be page aligned");

    acquire(&gp_vmmgr->lock);
    if (gp_vmmgr->malloc_top + size > gp_vmmgr->kstack_top) {
        release(&gp_vmmgr->lock);
        return 0;
    }
    start = gp_vmmgr->malloc_top;
    gp_vmmgr->malloc_top += size;
    release(&gp_vmmgr->lock);

    for (va = start; va < start + size; va += PAGE_SIZE) {
        ppn = pgalloc();
        if (ppn == 0)
            goto rollback;
        if (mmu_map_pages(kernel_pagetable, va, PAGE_SIZE, PPN2PA(ppn), PTE_R | PTE_W | PTE_G) != 0) {
            pgfree(ppn);
            goto rollback;
        }
    }
    return start;

rollback:
    if (va > start)
        mmu_user_unmap(kernel_pagetable, start, (va - start) / PAGE_SIZE, 1);
    return 0;
}

static kmem_slub_t*
__slubmap_add(uintptr_t addr)
{
    uintptr_t aslub;
    uint64_t ppn;

    int idx = virt2sidx(addr);
    aslub = PGROUNDDOWN((uintptr_t)&slub_map[idx]);
//    debug("[VMEM] slub index %d start at 0x%lX round down 0x%lX\n", idx, &slub_map[idx], aslub);
//    debug("[VMEM] slubstart 0x%lX slubtop 0x%lX heap start 0x%lX\n", gp_vmmgr->slubmap_start,
//                gp_vmmgr->slubmap_top, gp_vmmgr->kheap_start);
    acquire(&gp_vmmgr->lock);
    if( (VMEMMAP_START <= aslub)
        && (gp_vmmgr->vmemmap_top <= aslub) 
        && (gp_vmmgr->vmemmap_top < MALLOC_MAP)) {
//        debug("allocating page for slub map\n");

        ppn = pgalloc();
        if(ppn == 0){
            panic("[VMEM] can't allocate pgalloc");
        }
        memset((void *)PPN2DA(ppn), 0, PAGE_SIZE);
        if(mmu_map_pages(kernel_pagetable, aslub, PAGE_SIZE, (uint64_t)PPN2PA(ppn), PTE_R| PTE_W | PTE_G) != 0){
            pgfree(ppn);
            panic("[VMEM] kstack_alloc can not map stack");
        }
        gp_vmmgr->vmemmap_top += PAGE_SIZE;
    } else if(gp_vmmgr->vmemmap_top >= MALLOC_MAP){
        panic("[VMEM] OUT OF SLUB MAP TABLE");
        return NULL;
    }
    release(&gp_vmmgr->lock);
    return &slub_map[idx];
}

kmem_slub_t*
vm_new_slub(kmem_cache_t *cache)
{
    uintptr_t addr, *a;
    kmem_slub_t *slub;
    addr = vm_alloc(PAGE_SIZE);
//    debug("[VMEM] page for slub allocated at 0x%lX\n", addr);
    if(addr == 0) 
        return NULL;
    slub = __slubmap_add(addr);
//    debug("[VMEM] slub at 0x%lX\n", slub);
    if(slub == NULL)
        return NULL;
//    list_init(&slub->slublist);
    slub->slub_cache = cache;
    slub->nextfree = (void *)addr;
//    debug("[VMEM] nextfree at 0x%lX\n", slub->nextfree);
    for( a = (uintptr_t *)addr;
         a < (uintptr_t *)(addr + PAGE_SIZE - cache->size);
         a += cache->size/sizeof(uintptr_t))
    {
        *a = (size_t)a + cache->size;
//        debug("[VMEM] nextfree addr 0x%lX => 0x%lX\n", a, *a);
    }
        *a = 0;
//        debug("[VMEM] nextfree addr 0x%lX => 0x%lX\n", a, *a);
    return slub;
}

uintptr_t
vm_kstack_alloc(size_t size)
{
    uintptr_t mem, va;
    uint64_t ppn;

    if ((size % PAGE_SIZE) != 0)
        panic("VMEM kstack size should be page aligned");

    acquire(&gp_vmmgr->lock);
    if (gp_vmmgr->kstack_top - (size + PAGE_SIZE) < gp_vmmgr->malloc_top) {
        release(&gp_vmmgr->lock);
        return 0;
    }
    gp_vmmgr->kstack_top -= size + PAGE_SIZE;
    mem = gp_vmmgr->kstack_top + PAGE_SIZE;
    release(&gp_vmmgr->lock);

    for (va = mem; va < mem + size; va += PAGE_SIZE) {
        ppn = pgalloc();
        if (ppn == 0)
            goto rollback;
        if (mmu_map_pages(kernel_pagetable, va, PAGE_SIZE, PPN2PA(ppn), PTE_R | PTE_W) != 0) {
            pgfree(ppn);
            goto rollback;
        }
    }
    return mem;

rollback:
    if (va > mem)
        mmu_user_unmap(kernel_pagetable, mem, (va - mem) / PAGE_SIZE, 1);
    return 0;
}



void
vmem_init(void)
{
    gp_vmmgr = &vm_mgr;
    gp_vmmgr->malloc_top = MALLOC_MAP;
    gp_vmmgr->kstack_top = DIRMEM_MAP;
    gp_vmmgr->vmemmap_top = VMEMMAP_START;

    slub_map = (kmem_slub_t *)VMEMMAP_START;


    initlock(&gp_vmmgr->lock, "vmem manager");


}