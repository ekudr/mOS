#include <common.h>
#include <mmu.h>
#include <memory.h>
#include <sched.h>
#include <spinlock.h>

spinlock_t mmu_lock __ALIGN(16);

// static int (*__sbi_rfence)(int fid,
// 			   unsigned long start, unsigned long size,
// 			   unsigned long arg4, unsigned long arg5);

pte_t *
mmu_walk(pagetable_t pagetable, uint64_t va, int alloc)
{
    uint64_t ppn;
    // check is virtual address valid 
    if ((va >= USRMEM_TOP) && (va < KRENELMEM_START))
        panic("walk");

    for (int vpn = 2; vpn > 0; vpn--) {
        pte_t *pte = &pagetable[PX(vpn, va)];
        if (*pte & PTE_V){
            pagetable = (pagetable_t)PA2DA(PTE2PA(*pte));
        } else {
            if (!alloc || (ppn = pgalloc()) == 0)
                return 0;
            memset((void *)PPN2DA(ppn), 0, PAGE_SIZE);
            *pte = PPN2PTE(ppn) | PTE_V;
            pagetable = (pagetable_t)PPN2DA(ppn);
        }
    }

    return (pte_t *)&pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64_t
mmu_walk_addr(pagetable_t pagetable, uint64_t va)
{
    pte_t *pte;
    uint64_t pa;

    //  if(va >= MAXVA)
    //    return 0;

    pte = mmu_walk(pagetable, va, 0);
    if (pte == 0)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    //  if((*pte & PTE_U) == 0)
    //    return 0;
    pa = PTE2PA(*pte);
    return pa;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mmu_map_pages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm)
{
    uint64_t a, last;
    pte_t *pte;

    if (size == 0)
        panic("mappages: size");

    /* Test if this is a leaf PTE, if it is, set A+D even if they are not used
     * by the implementation.
     *
     * If not, clear A+D+U because the spec. says:
     * For non-leaf PTEs, the D, A, and U bits are reserved for future use and
     * must be cleared by software for forward compatibility.
     */
//    debug("[MMU] map pt 0x%lX va 0x%lX size 0x%lX pa 0x%lX\n", pagetable, va, size, pa);

        if (perm & PTE_LEAF_MASK)
        {
            perm |= (PTE_A | PTE_D);
        }
      else
        {
          perm &= ~(PTE_A | PTE_D | PTE_U);
        }
    acquire(&mmu_lock);
    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for (;;)
    {
        //        debug("mmu_map pt 0x%lX va 0x%lX \n", pagetable, a);
        if ((pte = mmu_walk(pagetable, a, 1)) == 0)
            return -1;

        if (*pte & PTE_V)
        {
            printf("addr 0x%lX ", a);
            panic("mappages: remap");
        }

        *pte = PA2PTE(pa) | perm | PTE_V;

        mmu_invalidate_tlb_by_vaddr(a);

        if (a == last)
            break;
        a += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
    sbi_remote_sfence_vma(va, size);
    release(&mmu_lock);
    return 0;
}

/*
 * Allocate free pages and map them starting at vaddr.
 * vaddr and size should be page-aligned.
*/
kerrno_t
mmu_memmap(pagetable_t pgtable, uint64_t vaddr, uint64_t size, int perm)
{
    uint64_t a, ppn;

    if ((vaddr % PAGE_SIZE) || (size % PAGE_SIZE))
        return -ENOALIGN;

    for (a = vaddr; a < (vaddr + size); a += PAGE_SIZE)
    {
        ppn = pgalloc();
        if (ppn == 0)
        {
            debug("[MMU] Can not allocate memory.  %s %s\n", __func__, __LINE__);
            return -ENOMEM;
        }
        memset((void *)PPN2DA(ppn), 0, PAGE_SIZE);
        if (mmu_map_pages(pgtable, a, PAGE_SIZE, PPN2PA(ppn), perm ) != 0)
        {
            pgfree(ppn);
            debug("[MMU] Can not allocate memory.  %s %s\n", __func__, __LINE__);
            return -ENOMEM;
        }
    }

    return SUCCESS;
}

/*
 *   Create an empty user page table.
 *   returns 0 if out of memory.
 */
pagetable_t
mmu_user_pt_create()
{
    pagetable_t pgtable;
    pgtable = (pagetable_t)PPN2DA(pgalloc());
    if (pgtable == 0)
        return 0;
    memset(pgtable, 0, PAGE_SIZE);
    return pgtable;
}

/*
 * Recursively free page-table pages.
 * All leaf mappings must already have been removed.
 */
void mmu_free_walk(pagetable_t pagetable)
{
    // there are 2^9 = 512 PTEs in a page table.
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
        {
            // this PTE points to a lower-level page table.
            uint64_t child = PTE2PA(pte);
            mmu_free_walk((pagetable_t)PA2DA(child));
            pagetable[i] = 0;
        }
        else if (pte & PTE_V)
        {
            panic("[MMU] Free walk: leaf");
        }
    }
    pgfree(DA2PPN(pagetable));
}

/*
 * Remove npages of mappings starting from va. va must be
 * page-aligned. The mappings must exist.
 * Optionally free the physical memory.
 */
void mmu_user_unmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
    uint64_t a;
    pte_t *pte;

    if ((va % PAGE_SIZE) != 0)
        panic("[MMU] Unmap user pages - not aligned");

    for (a = va; a < va + npages * PAGE_SIZE; a += PAGE_SIZE)
    {
        if ((pte = mmu_walk(pagetable, a, 0)) == 0)
            panic("[MMU] Unmap user pages - walk error");
        if ((*pte & PTE_V) == 0)
            panic("[MMU] Unmap user pages - not mapped");
        if (PTE_FLAGS(*pte) == PTE_V)
            panic("[MMU] Unmap user pages - not a leaf");
        if (do_free)
        {
            uint64_t pa = PTE2PA(*pte);
            pgfree(PA2PPN(pa));
        }
        *pte = 0;
    }
}

/*
 * Free user memory pages,
 * then free page-table pages.
 */
void mmu_user_pg_free(pagetable_t pagetable, uint64 sz)
{
    if (sz > 0)
        mmu_user_unmap(pagetable, 0, PGROUNDUP(sz) / PAGE_SIZE, 1);
    mmu_free_walk(pagetable);
}


/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
*/

/*
 * !!!REWRITE not corect address for unmap function
 * Supposed user address space from 0 to sz.
 * I used different schema. 
*/
uint64_t
mmu_user_vmdealloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz)
{
    if (newsz >= oldsz)
        return oldsz;

    if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
    {
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PAGE_SIZE;
        mmu_user_unmap(pagetable, PGROUNDUP(newsz), npages, 1);
    }

    return newsz;
}

/*
 * Allocate PTEs and physical memory to grow process from oldsz to
 * newsz, which need not be page aligned.  Returns new size or 0 on error.
*/
/*
 * !!!REWRITE not corect address for unmap function
 * Supposed user address space from 0 to sz.
 * I used different schema. 
*/
uint64_t
mmu_user_vmalloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz, int xperm)
{
    uint64_t ppn;
    uint64_t a;


    if (newsz < oldsz)
        return oldsz;

    oldsz = PGROUNDUP(oldsz);
    for (a = oldsz; a < newsz; a += PAGE_SIZE)
    {
        ppn = pgalloc();
        if (ppn == 0)
        {
            mmu_user_vmdealloc(pagetable, a, oldsz);
            return 0;
        }
        memset((void *)PPN2DA(ppn), 0, PAGE_SIZE);
        if (mmu_map_pages(pagetable, a, PAGE_SIZE, PPN2PA(ppn), PTE_R | PTE_U | xperm) != 0)
        {
            pgfree(ppn);
            mmu_user_vmdealloc(pagetable, a, oldsz);
            return 0;
        }
    }
    return newsz;
}



/*
 * mark a PTE invalid for user access.
 * used by exec for the user stack guard page.
*/
void
mmu_user_vmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = mmu_walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

/*
 * Copy from kernel to user.
 * Copy len bytes from src to virtual address dstva in a given page table.
 * Return 0 on success, -1 on error.
*/
int mmu_user_copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len)
{
    uint64_t n, va0, pa0;
    pte_t *pte;

    while (len > 0)
    {
        va0 = PGROUNDDOWN(dstva);
        if (va0 >= USRMEM_TOP)
            return -1;
        pte = mmu_walk(pagetable, va0, 0);
        if (pte == NULL || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 ||
            (*pte & PTE_W) == 0)
            return -1;
        pa0 = PTE2PA(*pte);
        n = PAGE_SIZE - (dstva - va0);
        if (n > len)
            n = len;
        memmove((void *)(PA2DA(pa0) + (dstva - va0)), src, n);

        len -= n;
        src += n;
        dstva = va0 + PAGE_SIZE;
    }
    return 0;
}

/*
 * Copy from user to kernel.
 * Copy len bytes to dst from virtual address srcva in a given page table.
 * Return 0 on success, -1 on error.
*/
int mmu_user_copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len)
{
    uint64_t n, va0, pa0;

    while (len > 0)
    {
        va0 = PGROUNDDOWN(srcva);
        pa0 = mmu_walk_addr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PAGE_SIZE - (srcva - va0);
        if (n > len)
            n = len;
        memmove(dst, (void *)(PA2DA(pa0) + (srcva - va0)), n);

        len -= n;
        dst += n;
        srcva = va0 + PAGE_SIZE;
    }
    return 0;
}

/*
 * Copy a null-terminated string from user to kernel.
 * Copy bytes to dst from virtual address srcva in a given page table,
 * until a '\0', or max.
 * Return 0 on success, -1 on error.
*/
int mmu_user_copyinstr(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t max)
{
    uint64_t n, va0, pa0;
    int got_null = 0;

    while (got_null == 0 && max > 0)
    {
        va0 = PGROUNDDOWN(srcva);
        pa0 = mmu_walk_addr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PAGE_SIZE - (srcva - va0);
        if (n > max)
            n = max;

        char *p = (char *)(PA2DA(pa0) + (srcva - va0));
        while (n > 0)
        {
            if (*p == '\0')
            {
                *dst = '\0';
                got_null = 1;
                break;
            }
            else
            {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PAGE_SIZE;
    }
    if (got_null)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}


static void mmu_free_pagetable1(pagetable_t pagetable)
{
    // there are 2^9 = 512 PTEs in a page table.
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
        {
            // this PTE points to a lower-level page table.
            uint64_t child = PTE2PA(pte);
//            debug("[MMU] free page table 0x%lX \n", child);
            pgfree(PA2PPN(child));
        }
    }
//    debug("[MMU] free page table 0x%lX \n", DA2PA(pagetable));
    pgfree(DA2PPN(pagetable));
}

void mmu_free_pagetable(pagetable_t pagetable)
{
    // there are 2^9 = 512 PTEs in a page table.
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
        {
            // this PTE points to a lower-level page table.
            uint64_t child = PTE2PA(pte);
            mmu_free_pagetable1((pagetable_t)PA2DA(child));
        }
    }
//    debug("[MMU] free page table 0x%lX \n", DA2PA(pagetable));
    pgfree(DA2PPN(pagetable));
}


void mmu_init(void)
{
    initlock(&mmu_lock, "mmu");
}