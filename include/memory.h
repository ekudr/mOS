#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>
#include <list.h>
#include <mmu.h>
#include <shmem.h>
#include <board_krnldata.h>

//#include "ipc.h"


extern pagetable_t kernel_pagetable;

struct kernel_map
{
    uint64_t virt_kernel;   // Virtual address for kernel start
    uint64_t phys_load;     // Physical address where kernel load
    uint64_t rel_offset;    // Kernel relocation offset
    uint64_t pa_va_off;     // Physical to Direct mapping address
    uint64_t uservec;       // Trampoline user vector mapped address
    uint64_t userret;       // Trampoline user return mapped address
    bool     sparsemem;
    uint64_t nmemblocks;    // Num memory blocks in board_memmap
    uint64_t pgmap_base;
    uint64_t pgmap_size;
    uint64_t asid_max;
};

extern struct kernel_map kernel_map;

#define KSTACK_SIZE         (2*PAGE_SIZE) 
#define MIN_CACHE_SIZE      32   




#define virt2sidx(x)    (((uint64_t)(x)-MALLOC_MAP)>>PAGE_SHIFT)

struct kmem_cache_info {
    char    *name;
    unsigned int size;
};

struct kmem_cache {
    spinlock_t  lock;
    uint64_t    size;
    void **freelist;
    struct kmem_slub *slub;
    list_head_t qslub;
    uint64_t    nslubs;
    char        *name;
};
typedef struct kmem_cache kmem_cache_t;

struct vmem_mgr
{
    spinlock_t lock;
    uintptr_t vmemmap_top;  // slub map top. grow up
    uintptr_t malloc_top;   // top malloc heap. grow up
    uintptr_t kstack_top;  // top of kernel stacks. grow down
    uintptr_t cma_mem_start;   // Start physical cma
    uintptr_t cma_mem_end;      // End physical cma
    uintptr_t cma_mem_top;      // Top allocated physical cma

};
typedef struct vmem_mgr vmem_mgr_t;

extern vmem_mgr_t *gp_vmmgr;

struct kmem_slub {
    kmem_cache_t *slub_cache;
    list_head_t slublist;
    void *nextfree;
};
typedef struct kmem_slub kmem_slub_t;

extern kmem_slub_t *slub_map;

static inline unsigned int __kcache_index(size_t size)
{
	if (!size)
		return 0;

	if (size <= MIN_CACHE_SIZE)
		return 0;

	if (size <=         64) return 1;
	if (size <=        128) return 2;
	if (size <=        256) return 3;
	if (size <=        512) return 4;
	if (size <=       1024) return 5;
	if (size <=   2 * 1024) return 6;
	if (size <=   4 * 1024) return 7;

	/* Will never be reached. Needed because the compiler may complain */
	return -1;
}

/*
 * KSTACK stractures
 */
struct kstack
{
    uintptr_t start;
    uint64_t size;
    list_head_t stacklist;
};

typedef struct kstack kstack_t;

struct kstack_manager
{
    spinlock_t lock;
    list_head_t free;
    // do I need to queue allocated stacks?
    list_head_t alloc;
};

typedef struct kstack_manager kstack_manager_t;

typedef enum {
    VMEM_NONE = 0,
    VMEM_NULL,
    VMEM_UNTYPED,
    VMEM_MNODE,
    VMEM_CODE,
    VMEM_MEM,
    VMEM_IO,
} vmem_type_t;

typedef struct vmem_block
{
    kobject_t       hdr;
    uint64_t start;
    uint32_t size;
    uint16_t type;
} vmem_block_t;


/*
 * Page structures
 */

struct page_item{
    list_head_t freelist;
    uint64_t ppn;
};

enum page_flags
{
    PG_locked   = 0x0001,
    PG_table    = 0x0002,
    PG_active   = 0x0004,
    PG_reserved = 0x0008,
    PG_buddy    = 0x0010,

};



typedef struct page
{
    unsigned long flags;
    union
    {
        kmem_slub_t kmem_slub;
        struct page_item buddy_page;
        struct shmem_page shmem_page;
    };
    
} page_t;

extern page_t *page_map;

enum mtype{
    MEM_IO,
    MEMORY,
    MEM_EXT,
    FRMBUF,
    MEM_CMA,

};

struct board_mmap{
    uint64_t    base;
    uint64_t    top;
    enum mtype  type;
    uint64_t    map_addr;
    uint64_t    map_size;
    uint64_t    base_pfn;
};

extern struct board_mmap board_memmap[];

/*
 * mem_struct
 */

typedef struct mem_struct
{
    spinlock_t  lock;
    uint64_t    task_size;
    uint64_t    mmap_base;
    uint64_t    mmap_addr;
    uint64_t    start_brk;
    uint64_t    brk;
    uint64_t    start_stack;
    uint64_t    stacktop;
    uint64_t    start_code;
    uint64_t    end_code;

    list_head_t         memlist;
} mem_struct_t;


typedef enum {
    MM_REG_VALID    = 0x00000001,
    MM_REG_MEM      = 0x00000010,
    MM_REG_MMIO     = 0x00000020,
    MM_REG_STACK    = 0x00000030,
} memreg_status_t;

#define MM_REG_STATUS_TYPE_MASK 0x0000000F0

typedef struct mem_region
{
    kobject_t       hdr;
    uint64_t        addr;
    uint64_t        size;
    memreg_status_t status;
    list_head_t     memlist;
    union {
        struct shmem_block  *shmem_block;
    };
} mem_reg_t;

typedef struct dma_mem_block
{
    kobject_t       hdr;
    uint64_t        addr;
    uint64_t        size;    
} dma_mem_block_t;


#define SECTION_SHIFT 56
#define SECTION_MASK 0xFF

#define section_to_flag(s)  (((uint64_t)s & SECTION_MASK) << SECTION_SHIFT)
#define flag_to_section(f)  ((f >> SECTION_SHIFT) & SECTION_MASK)

#ifdef BOARD_SPARSEMEM
inline page_t *ppn_to_page(uint64_t ppn)
{
    for (int i = 0; i < kernel_map.nmemblocks; i++)
    {
        if ((board_memmap[i].type != MEMORY) && 
        (board_memmap[i].type != MEM_EXT) && (board_memmap[i].type != FRMBUF))
            continue;          
            
        if ((board_memmap[i].base_pfn <= ppn) && 
            (ppn <= (board_memmap[i].top >> PAGE_SHIFT)))
            return (page_t *)board_memmap[i].map_addr + (ppn - board_memmap[i].base_pfn);    
    }

    return NULL;
}
inline uint64_t page_to_ppn(page_t *page)
{
    page_t *p = page;
    uint64_t sec = (p->flags >> SECTION_SHIFT) & SECTION_MASK;
    uint64_t ppn = (p - (page_t *)board_memmap[sec].map_addr) + board_memmap[sec].base_pfn;
    return ppn;
}
#else
#define ppn_to_page(pfn)   (page_map + (pfn - BASE_PFN))
#define page_to_ppn(p)   (((page_t *)p - page_map) + BASE_PFN)
#endif /* BOARD_SPARSEMEM */

#define lock_page(pfn) ppn_to_page(pfn)->flags |= PG_locked

/*
 * Memory map structures
 */

enum{
    MAP_SHARED  = 0x0001,
    MAP_PRIVATE = 0x0002,
    MAP_MEMIO   = 0x0004,
    MAP_READ    = 0x0010,
    MAP_WRITE   = 0x0020,
    MAP_EXEC    = 0x0040,
};



void vm_init(void);
void vmem_init(void);
void kmem_init(void);
uint64_t kmem_cma_alloc(size_t size);

int mmu_map_pages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm) ;

uintptr_t vm_kstack_alloc(size_t size);
kmem_slub_t* vm_new_slub(kmem_cache_t *cache);

kstack_t *kstack_alloc(void);
int kstack_free(kstack_t *ks);

void pg_pool_init(void);
void pg_free_range(uint64_t start, uint64_t end);
uint64_t pgalloc(void);
void pgfree(uint64_t ppn);





#endif /* __MEMORY_H__ */