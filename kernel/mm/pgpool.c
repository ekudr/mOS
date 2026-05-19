#include <common.h>
#include <spinlock.h>
#include <list.h>
#include <mmu.h>
#include <memory.h>
#include <riscv.h>

page_t *page_map;


struct {
    spinlock_t lock;
    list_head_t qfree;
    uint64_t nfree;
} pg_pool;

/* 
 * Free the page of physical memory pointed at by direct map va.
 */
void pgfree(uint64_t ppn)
 {
    uint64_t    f;
    struct page_item *p;
/*
    if (!ppn_valid(ppn))
    {
        panic("pg_free");
    }
*/
    f = ppn_to_page(ppn)->flags;
    f &= ~PG_active;
//    f |= PG_buddy; 
    ppn_to_page(ppn)->flags = f;
//    p = (struct page_item *)va;
    p = &ppn_to_page(ppn)->buddy_page;
//    debug("[PAGEPOOL] free page ppn 0x%lX page_t at 0x%lX\n", ppn, p);
    p->ppn = ppn;
//    debug("[PAGEPOOL] free page ppn 0x%lX page_t at 0x%lX\n", p->ppn, p);
    acquire(&pg_pool.lock); 
    list_add_tail(&pg_pool.qfree, &p->freelist);
    pg_pool.nfree++; 
    ppn_to_page(p->ppn)->flags |= PG_buddy;   
    release(&pg_pool.lock);
}

void 
pg_free_range(uint64_t start, uint64_t end) 
{
    char *p;
//    debug("Page free range 0x%lX->0x%lX\n", start, end);
    p = (char*)PGROUNDUP(start);
    for(; p + PAGE_SIZE <= (char*)end; p += PAGE_SIZE)
        pgfree(DA2PPN(p));
}

/* Init free pages pool*/
void 
pg_pool_init() 
{
    // Init page_map[]
    page_map    = (page_t *)PGMAP_START;
    uint64_t pm = (uint64_t)page_map;
    uint64_t type, count;
    count = 0;
    for (int i = 0; i < kernel_map.nmemblocks; i++)
    {
        type = board_memmap[i].type;
        uint64_t pa_start = board_memmap[i].base;
        uint64_t pa_end   = board_memmap[i].top;        
        if ((type == MEMORY) || (type == MEM_EXT))
        {
            board_memmap[i].base_pfn = pa_start >> PAGE_SHIFT;
            board_memmap[i].map_addr = pm;
            early_printf("Memblock %d 0x%lX-0x%lX 0x%lX bytes\n", i, pa_start, pa_end, pa_end- pa_start);
            early_printf("page_map size 0x%lX\n", board_memmap[i].map_size);
            for (int p = pa_start >> PAGE_SHIFT; p <= pa_end >> PAGE_SHIFT; p++)
            {
//                debug("PFN 0x%lX page at 0x%lX\n", p, pfn_to_page(p));
                ppn_to_page(p)->flags = section_to_flag(i);
                count++;
//                page_map[p-(pa_start >> PAGE_SHIFT)].flags = PG_unknown;
            }
            pm += board_memmap[i].map_size;
            early_printf("Page_map %d at 0x%lX range 0x%lX->0x%lX base pfn 0x%lX\n", i, board_memmap[i].map_addr,
                    board_memmap[i].base, board_memmap[i].top, board_memmap[i].base_pfn);
        }  else if (type == FRMBUF){
            board_memmap[i].base_pfn = pa_start >> PAGE_SHIFT;
            board_memmap[i].map_addr = pm;
            early_printf("Memblock %d 0x%lX-0x%lX 0x%lX bytes\n", i, pa_start, pa_end, pa_end- pa_start);
            early_printf("page_map at 0x%lX\n", board_memmap[i].map_addr);            
            for (int p = pa_start >> PAGE_SHIFT; p <= pa_end >> PAGE_SHIFT; p++)
            {
//                debug("PPN 0x%lX page at 0x%lX\n", p, ppn_to_page(p));
                ppn_to_page(p)->flags = section_to_flag(i) | PG_reserved;
                count++;
//                page_map[p-(pa_start >> PAGE_SHIFT)].flags = PG_unknown;
            }
            pm += board_memmap[i].map_size;
            early_printf("Page_map %d at 0x%lX range 0x%lX->0x%lX base pfn 0x%lX\n", i, board_memmap[i].map_addr,
                board_memmap[i].base, board_memmap[i].top, board_memmap[i].base_pfn);            
        }          
    }
    early_printf("0x%lX pages initiated\n", count);

    
    initlock(&pg_pool.lock, "pgmem");
    list_init(&pg_pool.qfree);
    pg_pool.nfree = 0;    

}


/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a PPN.
 * Returns 0 if the memory cannot be allocated.
 */ 
uint64_t 
pgalloc(void) 
{

    struct page_item *p;
    uint64_t    f, ppn;

    if(list_is_empty(&pg_pool.qfree))
        return 0;

        
    acquire(&pg_pool.lock);
//    debug("Next free at 0x%lX\n", pg_pool.qfree.next);
    p = list_first_entry(&pg_pool.qfree, struct page_item, freelist);
//    debug("[PAGEPOOL] alloc page ppn 0x%lX page_t at 0x%lX\n", p->ppn, p); 
//debug("Physical address 0x%lX\n",mmu_walk_addr(kernel_pagetable, p)); 
    list_del(&p->freelist);
//    debug("Page allocate PPN 0x%lX DA 0x%lX\n", p->ppn, p);
    ppn = p->ppn;
//   debug("[PAGEPOOL] alloc page ppn 0x%lX page_t at 0x%lX\n", ppn, p);
    f = __atomic_load_n(&ppn_to_page(ppn)->flags, __ATOMIC_ACQUIRE);
//    f = READ_ONCE(ppn_to_page(ppn)->flags);
    f &= ~PG_buddy;
    f |= PG_active; 
    __atomic_store_n(&ppn_to_page(ppn)->flags, f, __ATOMIC_RELEASE);
    release(&pg_pool.lock);
    
//    debug("[PGALLOC] ppn 0x%lX page_t 0x%lX flags 0x%lX\n", ppn, p, ppn_to_page(p->ppn)->flags);
        
//    memset((void *)PPN2DA(ppn), 0, PAGE_SIZE);
    return ppn;
}

