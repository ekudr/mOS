#ifndef __MMU_H__
#define __MMU_H__

#define USRMEM_TOP      (1##ULL<<38)            /* Top of user address space */
#define KRENELMEM_START (-1##ULL<<38)
#define KERNELMEM_TOP   (-1##ULL)
#define KERMEL_MAP      (-0x80000000##ULL)
#define DIRMEM_MAP      (-0x2A00000000##ULL)
#define MALLOC_MAP      (-0x3A00000000##ULL)
#define VMEMMAP_START   (-0x3B00000000##ULL)
#define PGMAP_START     (-0x3C00000000##ULL)

#define USRMEMMAP_START  (0x1000000000##ULL)   /* Address in user space to map phis memory*/


/* RV64 Sv39 MMU*/

#define PAGE_SHIFT       (12)
#define PAGE_SIZE        (1 << PAGE_SHIFT) /* 4K pages */
#define PAGE_MASK        (PAGE_SIZE - 1)

#define PGROUNDUP(sz)           (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PGROUNDDOWN(a)          (((a)) & ~(PAGE_SIZE-1))

/* Common Page Table Entry (PTE) bits */

#define PTE_V                   (1 << 0) /* PTE is valid */
#define PTE_R                   (1 << 1) /* Page is readable */
#define PTE_W                   (1 << 2) /* Page is writable */
#define PTE_X                   (1 << 3) /* Page is executable */
#define PTE_U                   (1 << 4) /* Page is a user mode page */
#define PTE_G                   (1 << 5) /* Page is a global mapping */
#define PTE_A                   (1 << 6) /* Page has been accessed */
#define PTE_D                   (1 << 7) /* Page is dirty */

/* Check if leaf PTE entry or not (if X/W/R are set it is) */

#define PTE_LEAF_MASK           (7 << 1)

#define SATP_MODE_SV39          (8##UL)

#define SATP_PPN_WIDTH        44
#define SATP_ASID_WIDTH       16
#define SATP_MODE_WIDTH       4

/* Supervisor Address Translation and Protection (satp) */

#define SATP_PPN_SHIFT          (0)
#define SATP_PPN_MASK           (((1ul << SATP_PPN_WIDTH) - 1) << SATP_PPN_SHIFT)
#define SATP_ASID_SHIFT         (SATP_PPN_WIDTH)
#define SATP_ASID_MASK          (((1ul << SATP_ASID_WIDTH) - 1) << SATP_ASID_SHIFT)
#define SATP_MODE_SHIFT         (SATP_PPN_WIDTH + SATP_ASID_WIDTH)
#define SATP_MODE_MASK          (((1ul << SATP_MODE_WIDTH) - 1) << SATP_MODE_SHIFT)


// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64_t)(pa)) >> PAGE_SHIFT) << 10)
#define PTE2PA(pte) (((pte) >> 10) << PAGE_SHIFT)

/* Convert a physical address to PPN */
#define PA2PPN(pa)      (((uint64_t)(pa)) >> PAGE_SHIFT)
#define PPN2PA(ppn)     (((uint64_t)(ppn)) << PAGE_SHIFT)

/* Convert PPN to PTE */
#define PPN2PTE(ppn)    (((uint64_t)(ppn)) << 10)

/* Convert PPN to direct mapped address*/
#define PPN2DA(ppn) ((((uint64_t)(ppn)) << 12) + DIRMEM_MAP)

#define DA2PPN(da)  ((((uint64_t)(da)) - DIRMEM_MAP) >> PAGE_SHIFT)

/* Convert a physical address to/from direct mapped address*/
#define PA2DA(pa)   (((uint64_t)(pa)) + DIRMEM_MAP)
#define DA2PA(da)   (((uint64_t)(da)) - DIRMEM_MAP)

#define KA2PA(da)   (((uint64_t)(da)) - KERMEL_MAP)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

/* get VPN[x] from Virtul address */
#define PXMASK          0x1FF /* 9 bits */ 
#define PXSHIFT(x)  (PAGE_SHIFT+(9*(x)))
#define PX(x, va) ((((uint64_t)(va)) >> PXSHIFT(x)) & PXMASK)


/*
 * map the trampoline page to the highest address,
 * in both user and kernel space.
*/
#define TRAMPOLINE (USRMEM_TOP  - PAGE_SIZE)

/*
// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
 *   USERSTACK (grows down)
 *   TRAPFRAME (t->trapframe, used by the trampoline)
 *   TRAMPOLINE (the same page as in the kernel)
 */
#define TRAPFRAME (TRAMPOLINE - PAGE_SIZE)

/*
 * User stack start.
 * Grows down.
 * One page is a security gap.
 */
#define USERSTACK (TRAPFRAME - 2 * PAGE_SIZE)

#ifndef __ASSEMBLY__

typedef uint64_t pte_t;
typedef uint64_t *pagetable_t; 



extern unsigned char     _start[];
extern unsigned char     _end[];
extern unsigned char     _stack_top[];
extern unsigned char     _data_loc[];
extern unsigned char     _pgtable_start[];
extern unsigned char     _servers_img[];
extern unsigned char     _servers_img_end[];

extern unsigned char     trampoline[]; // trampoline.S
extern unsigned char     uservec[];
extern unsigned char     userret[];
extern unsigned char     usersignal[];
extern unsigned char     usersignalret[];

/* Prepare register to satp. input direct address page table and asid */
static inline uint64_t 
mmu_satp_reg(uint64_t pgbase, uint16_t asid)
{
  uint64_t reg;
  reg  = ((SATP_MODE_SV39 << SATP_MODE_SHIFT) & SATP_MODE_MASK);
  reg |= (((uint64_t)asid << SATP_ASID_SHIFT) & SATP_ASID_MASK);
  reg |= ((DA2PPN(pgbase) << SATP_PPN_SHIFT) & SATP_PPN_MASK);
  return reg;
}


static inline void 
mmu_write_satp(uint64_t reg)
{
    __asm__ __volatile__
        (
            "csrw satp, %0\n"
            "sfence.vma x0, x0\n"
            "fence rw, rw\n"
            "fence.i\n"
            : : "rK" (reg) : "memory"
        );
}
static inline uint64_t 
mmu_read_satp(void)
{
    uint64_t reg;
    __asm__ __volatile__
        (
            "csrr %0, satp\n"
            : "=r" (reg) : : "memory"
        );
    return reg;
}
static inline unsigned int 
mmu_check_asid(void)
{
    uint64_t reg ;
    reg = mmu_read_satp();
    reg |= (((uint64_t)-1) & (~((1ULL << 44)-1)));
    mmu_write_satp(reg);
    reg = mmu_read_satp();
    return ((reg >> 44) & ((1ULL << 16) - 1));
}
static inline void 
mmu_invalidate_tlb_by_vaddr(uintptr_t vaddr)
{
  __asm__ __volatile__
    (
      "sfence.vma %0, x0\n"
      :
      : "rK" (vaddr)
      : "memory"
    );
}

static inline void 
mmu_invalidate_tlbs(void)
{
  __asm__ __volatile__
    (
      "sfence.vma x0, x0\n"
      :
      :
      : "memory"
    );
}

static inline void 
mmu_switch_pagetable(uint64_t pgbase, uint16_t asid)
{
    uint64_t reg = mmu_satp_reg(pgbase, asid);
    /* Commit to satp and synchronize */
    mmu_write_satp(reg);
}

pagetable_t mmu_user_pt_create();
uint64_t mmu_walk_addr(pagetable_t pagetable, uint64_t va);
void mmu_free_walk(pagetable_t pagetable);
void mmu_user_unmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);
void mmu_user_pg_free(pagetable_t pagetable, uint64 sz);
uint64_t mmu_user_vmalloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz, int xperm);
uint64_t mmu_user_vmdealloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz);
void mmu_user_vmclear(pagetable_t pagetable, uint64 va);
int mmu_user_copyinstr(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t max);
int mmu_user_copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len);
int mmu_user_copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len);
kerrno_t mmu_memmap(pagetable_t pgtable, uint64_t vaddr, uint64_t size, int perm);

void mmu_free_pagetable(pagetable_t pagetable);
void mmu_init(void);

// ASID allocator (kernel/mm/asid.c)
struct mem_struct;
void     asid_init(void);
uint16_t asid_alloc(struct mem_struct *mm);
void     asid_free(struct mem_struct *mm);

int mmu_move_pages(pagetable_t from, pagetable_t to, uint64_t va_src,
                    uint64_t va_dst, uint64_t len, int perm);

// Look up va in pagetable. On success returns 0 and fills *pa with the
// physical address and *perm with the PTE permission bits (R|W|X|U).
// Returns -1 if the page is not mapped.
int mmu_walk_pte(pagetable_t pagetable, uint64_t va, uint64_t *pa, int *perm);

// Map pages from `from` into `to` at the same VA without unmapping from `from`
// (unlike mmu_move_pages, which empties the source). Caller is responsible
// for ensuring frames remain valid for the lifetime of the destination
// mapping — there is no refcount.
int mmu_share_pages(pagetable_t from, pagetable_t to, uint64_t va, uint64_t len);


void mmu_pt_dump(pagetable_t pt);

#endif /* __ASSEMBLY__ */
#endif /* __MMU_H__ */