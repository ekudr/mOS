#define __BOARD_MEMMAP__
#include <common.h>
#include <spinlock.h>
#include <mmu.h>
#include <memory.h>



pagetable_t early_pagetable;
pagetable_t kernel_pagetable;
uint64_t    kpgt;

struct kernel_map kernel_map;


uint64_t boot_hartid;
uint64_t __a1;
uint64_t __a2;
uint64_t __a3;
uint64_t __a4;
uint64_t __a5;
uint64_t __a6;
uint64_t __a7;

uint64_t mem_start, mem_end;

void
lib_puts(char *s) {  
    while (*s) {
        sbi_putc(*s++);
    }
}

static uint64_t next_page;

static void
init_pages(void)
{
    next_page = mem_start;
}

uint64_t
get_page(void)
{
    uint64_t pg = next_page;
    if(pg >= mem_end)
        return 0;
    next_page += 0x1000;
    return pg;
}



pte_t *
early_mmu_walk(pagetable_t pagetable, uint64_t va, int alloc)
{
    for(int vpn = 2; vpn > 0; vpn--) {
//        early_printf("mmu walk lv %d va 0x%lX pg 0x%lX idx %d\n", vpn, va, pagetable, PX(vpn, va));
        pte_t *pte = &pagetable[PX(vpn, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if(!alloc || (pagetable = (pte_t*)get_page()) == 0)
                return 0;
            memset((void *)pagetable, 0, PAGE_SIZE);
//            early_printf("pg alloc 0x%lX\n", pagetable);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

void
early_vm_map(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, uint64_t mmuflags)
{
    pte_t   *pte;
    uint64_t a,last;
//    early_printf("Early map pt 0x%lX va 0x%lX size 0x%lX pa 0x%lX\n", pagetable, va, size, pa);
    mmuflags |= (PTE_A | PTE_D);

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for(;;){
        if((pte = early_mmu_walk(pagetable, a, 1)) == 0)
            panic("mappages: can not map");
        if(*pte & PTE_V){
            debug("Address 0x%lX pte 0x%lX pa 0x%lX\n", a, pte, PTE2PA(*pte));
            panic("mappages: remap");
        }
        
        *pte = PA2PTE(pa) | mmuflags | PTE_V;
//        early_printf("Address 0x%lX pte 0x%lX pa 0x%lX\n", a, pte, PTE2PA(*pte));
        if(a == last)
            break;
        a += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
}

static void
early_mem_dirmap(pagetable_t pagetable,uint64_t va, uint64_t mem_top)
{
    early_printf("Direct mapping Vaddr 0x%lX \n", va);
    for(uint64_t pa=0; pa < mem_top; pa += 0x40000000){
        pagetable[PX(2, va+pa)] = PA2PTE(pa) | PTE_V | PTE_R | PTE_W | PTE_G | PTE_A | PTE_D;
    }
}

void 
early_vm_init(void)
{
    
    mem_start = PGROUNDUP((uint64_t)_stack_top + 0x1000 + (0x1000 * NCPUS));
    mem_end = MEMMAP_TOP;

    kernel_map.virt_kernel = KERMEL_MAP; 
    kernel_map.phys_load = (uint64_t)_start;
    kernel_map.rel_offset = kernel_map.virt_kernel - kernel_map.phys_load;
    kernel_map.pa_va_off = DIRMEM_MAP;
    kernel_map.uservec = TRAMPOLINE + ((uint64_t)uservec - (uint64_t)trampoline);
    kernel_map.userret = TRAMPOLINE + ((uint64_t)userret - (uint64_t)trampoline);

    kernel_map.asid_max = mmu_check_asid();
 
    init_pages();

    // Allocate physical linear memory for page_map[]
    // It's easy to map later
#ifdef SPARSEMEM
    kernel_map.sparsemem = true;
#else
    kernel_map.sparsemem = false;
#endif
    kernel_map.nmemblocks = NELEM(board_memmap);

    size_t      size, pgm_size = 0;
    uint64_t    va = next_page;   
    enum mtype  type;
    kernel_map.pgmap_base = va;
    
    early_printf("Size of page_t 0x%lX\n", sizeof(page_t));
    for (int i = 0; i < NELEM(board_memmap); i++)
    {
        type = board_memmap[i].type;
        if ((type == MEMORY) || (type == MEM_EXT) || (type == FRMBUF))
        {
            size = board_memmap[i].top - board_memmap[i].base;
            size = PGROUNDUP(size);
            early_printf("Memory block size 0x%lX\n", size);
            size = size >> PAGE_SHIFT;
            early_printf("Memory block 0x%lX pages\n", size);
            size *= sizeof(page_t);
            early_printf("page_map at 0x%lX size 0x%lX\n", va, size);
            board_memmap[i].map_addr = va;
            board_memmap[i].map_size = size;
            va += size;
            pgm_size += size;
        }
    }

    kernel_map.pgmap_size = pgm_size;
    next_page = PGROUNDUP(va);

    early_pagetable = (uint64_t *)get_page();
    memset(early_pagetable, 0, PAGE_SIZE);

    

    early_mem_dirmap(early_pagetable, DIRMEM_MAP, MEMMAP_TOP);
    

    // map text and rodata sections as RX
    early_vm_map(early_pagetable, kernel_map.virt_kernel, 
                _data_loc - _start, KLOADADDR, (PTE_R | PTE_X | PTE_G));
    early_vm_map(early_pagetable, kernel_map.virt_kernel + (_data_loc - _start),
                 mem_start - (uint64_t)_data_loc,
                 KLOADADDR + (_data_loc - _start) ,
                 (PTE_R | PTE_W | PTE_G));

    // map page_map[]

        early_vm_map(early_pagetable, PGMAP_START, kernel_map.pgmap_size,
                        kernel_map.pgmap_base, (PTE_R | PTE_W | PTE_G));

}
/*
int 
memory_map(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, uint64_t perm)
{
            if((board_memmap[i].base % 0x40000000) &&
                ((board_memmap[i].top - board_memmap[i].base + 1) % 0x40000000)){
                for(uint64_t pa = board_memmap[i].base;
                     pa < board_memmap[i].top; pa += 0x40000000)
                    kernel_pagetable[PX(2, DIRMEM_MAP + pa)] = 
                                    PA2PTE(pa) | PTE_V | PTE_R | PTE_W | PTE_G | PTE_A | PTE_D;    
            } else {
                mmu_map_pages(kernel_pagetable, DIRMEM_MAP + board_memmap[i].base,
                                board_memmap[i].top - board_memmap[i].base,
                                board_memmap[i].base,
                                (PTE_R | PTE_W | PTE_G));
            }
}
*/

void
vm_init(void)
{
//     early_printf("a1 0x%lX a2 0x%lX a3 0x%lX a4 0x%lX\n", __a1, __a2, __a3, __a4);
//     early_printf("a5 0x%lX a6 0x%lX a7 0x%lX\n", __a5, __a6, __a7);
//     uint32_t *dtb = (uint32_t *)PA2DA(__a1);
//     char *str = (char *)PA2DA(dtb[0]);
// //    early_printf("[sadasd] %s\n", str);
//     for (int i = 0; i < 50; i++)
//     {
//         early_printf("0x%x ", str[i]);
//     }
    
    
    // printf("Kernel memory map:\n");
    // printf("     VMEMMAP: 0x%p\n", VMEMMAP_START);
    // printf("     MALLOC:  0x%p\n", MALLOC_MAP);
    // printf("     DIRMEM:  0x%p\n", DIRMEM_MAP);
    // printf("     KERNEL:  0x%p\n", KERMEL_MAP);
    early_printf("Kernel virtual start:  0x%p\n", kernel_map.virt_kernel);
    early_printf("Kernel physical start:  0x%p\n", kernel_map.phys_load);
    early_printf("Kernel relocation offset:  0x%p\n", kernel_map.rel_offset);
    early_printf("Page tables start:  0x%p\n", (uint64_t)_pgtable_start - kernel_map.rel_offset);

    mmu_init();

    pg_pool_init();

    for (int i = 0; i < NELEM(board_memmap); i++)
    {
        if (board_memmap[i].type == MEMORY)
        {
            if ((next_page > board_memmap[i].base) &&
                (next_page < board_memmap[i].top))
            {
                pg_free_range(next_page + DIRMEM_MAP, board_memmap[i].top + DIRMEM_MAP);
            }
            else
            {
                pg_free_range(board_memmap[i].base + DIRMEM_MAP, board_memmap[i].top + DIRMEM_MAP);
            }
        }
        else if (board_memmap[i].type == MEM_EXT)
        {
            pg_free_range(board_memmap[i].base + DIRMEM_MAP, board_memmap[i].top + DIRMEM_MAP);
        }
    }

    kernel_pagetable = (pagetable_t)PPN2DA(pgalloc());
    memset(kernel_pagetable, 0, PAGE_SIZE);
    
    // map text and rodata sections as RX
    mmu_map_pages(kernel_pagetable, kernel_map.virt_kernel, 
                _data_loc - _start, kernel_map.phys_load, (PTE_R | PTE_X | PTE_G));
    // map data sections as RW
    mmu_map_pages(kernel_pagetable, (uint64_t)_data_loc,
                 mem_start - kernel_map.phys_load + (_data_loc - _start),
                 kernel_map.phys_load + (_data_loc - _start) ,
                 (PTE_R | PTE_W | PTE_G));


    // ??? REWRITE Map only RAM in direct mapping
    // MEMIO map on demand              
    early_printf("Memory mapping:\n");
    for(int i=0; i<NELEM(board_memmap); i++){
        early_printf("    0x%lX -> 0x%lX type %d\n", 
                board_memmap[i].base, board_memmap[i].top, board_memmap[i].type);
      
        if((board_memmap[i].type == MEM_IO) || (board_memmap[i].type == MEM_CMA) ||
            (board_memmap[i].type == FRMBUF)){
            mmu_map_pages(kernel_pagetable, DIRMEM_MAP + board_memmap[i].base,
                        board_memmap[i].top - board_memmap[i].base,
                       board_memmap[i].base, (PTE_R | PTE_W | PTE_G));  
        } else if(board_memmap[i].type == MEMORY){
            if ((board_memmap[i].base <= (uint64_t)_pgtable_start - kernel_map.rel_offset) &&
                ((uint64_t)_pgtable_start - kernel_map.rel_offset < board_memmap[i].top))
            {
                mmu_map_pages(kernel_pagetable, (uint64_t)_pgtable_start - kernel_map.rel_offset + DIRMEM_MAP,
                            board_memmap[i].top - ((uint64_t)_pgtable_start - kernel_map.rel_offset),
                            (uint64_t)_pgtable_start - kernel_map.rel_offset, (PTE_R | PTE_W | PTE_G));                
            }
            
        } else if(board_memmap[i].type == MEM_EXT){
            mmu_map_pages(kernel_pagetable, DIRMEM_MAP + board_memmap[i].base,
                        board_memmap[i].top - board_memmap[i].base,
                        board_memmap[i].base, (PTE_R | PTE_W | PTE_G));  
        }
    

    }

    // map page_map[]
    mmu_map_pages(kernel_pagetable, PGMAP_START, kernel_map.pgmap_size,
                        kernel_map.pgmap_base, (PTE_R | PTE_W | PTE_G));
    
    // map the trampoline for trap entry/exit to
    // the highest virtual address in low segment.
    // The trampoline has same address for kernel and user address spaces.
    mmu_map_pages(kernel_pagetable, TRAMPOLINE, PAGE_SIZE, (uint64_t)trampoline - kernel_map.rel_offset, PTE_R | PTE_X | PTE_G);

    mmu_switch_pagetable((uint64_t)kernel_pagetable, 0); 

    kpgt = DA2PA(kernel_pagetable);

    mmu_free_pagetable((pagetable_t)PA2DA(early_pagetable));

    vmem_init();
    kmem_init();

}


uint64_t
get_next(void)
{
    return next_page;
}