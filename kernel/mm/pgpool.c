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
    struct page_item *p;

    p = &ppn_to_page(ppn)->buddy_page;
    p->ppn = ppn;
    acquire(&pg_pool.lock);
    ppn_to_page(ppn)->flags &= ~PG_active;
    ppn_to_page(ppn)->flags |= PG_buddy;
    list_add_tail(&pg_pool.qfree, &p->freelist);
    pg_pool.nfree++;
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
    uint64_t ppn;

    acquire(&pg_pool.lock);
    if (list_is_empty(&pg_pool.qfree)) {
        release(&pg_pool.lock);
        return 0;
    }
    p = list_first_entry(&pg_pool.qfree, struct page_item, freelist);
    list_del(&p->freelist);
    ppn = p->ppn;
    ppn_to_page(ppn)->flags &= ~PG_buddy;
    ppn_to_page(ppn)->flags |= PG_active;
    pg_pool.nfree--;
    release(&pg_pool.lock);
    return ppn;
}

