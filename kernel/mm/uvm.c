#include <common.h>
#include <mmu.h>
#include <memory.h>
#include <sched.h>
#include <errno.h>

/*
 * Allocate memory region in given task.
 */
kerrno_t
uvm_alloc_mmreg(task_t *task, uint64_t vaddr, uint64_t size, uint64_t type, int xperm)
{
    list_head_t     *pos;
    mem_reg_t       *mreg, *mreg_stack = NULL;
    task_t          *t;
    uint64_t        a, ppn;
    mem_struct_t    *mm;
    pagetable_t     pgtable;
    bool            exist = false;


    // vaddr and size shoud be round up to page size 
    vaddr   = PGROUNDDOWN(vaddr);
    size    = PGROUNDUP(size);

    t           = task;    
    mm          = t->mm;
    pgtable     = mm->pagetable;

//    debug("[UVM] Alloc mem reg task %d va 0x%lX size 0x%lX type 0x%lX\n",
//            t->pid,vaddr, size, type);

    acquire(&mm->lock);
    // Allocate the stack anyway.
    // If stack is exist it should be extend it.

    // search if addr is already allocated
    list_for_each(pos, &mm->memlist)
    {
        mreg = list_entry(pos, mem_reg_t, memlist);
        if((mreg->addr <= vaddr) && (vaddr < mreg->addr + mreg->size))
        {
            exist = true;
        }
        if ((mreg->status & MM_REG_STATUS_TYPE_MASK) == MM_REG_STACK)
        {
            mreg_stack = mreg;
        }
    }

    release(&mm->lock);
    if (exist)
    {
        panic("[MMU] Memory region is allocated already");
        
        return -EEXIST;
    }

    if(type == MM_REG_STACK)
    {
        if (mreg_stack != NULL)
        {
            mreg_stack->addr = __atomic_sub_fetch(&mm->stacktop, size, __ATOMIC_ACQ_REL);
            mreg_stack->size += size;
            if (mmu_memmap(pgtable, mreg_stack->addr, size, PTE_U | PTE_R | PTE_W) != SUCCESS)
            {
                // !!! undo changes if no panic
                panic("[UVM] can not allocate stack");
                return -ENOMEM;
            }
            return SUCCESS;
        }
        vaddr = __atomic_sub_fetch(&mm->stacktop, size, __ATOMIC_ACQ_REL);
    }

    mreg = (mem_reg_t *)malloc(sizeof(mem_reg_t));
    if(mreg == NULL){
        panic("[MMU] Can not allocate a mem block");
        return -ENOMEM;
    }
        

    mreg->addr = vaddr;
    mreg->size = size;

    if (mmu_memmap(pgtable, vaddr, size, PTE_R | PTE_U | xperm) != SUCCESS)
    {
        mfree(mreg);
        // !!! undo stacktop if MM_REG_STACK and no panic
        panic("[UVM] can not allocate memory");
        return -ENOMEM;     
    }
    acquire(&mm->lock);
    mreg->status = type | MM_REG_VALID;
    list_add(&mm->memlist, &mreg->memlist);

    if(type != MM_REG_STACK)
    {
        if ((mm->start_code == 0) || (vaddr < mm->start_code))
            mm->start_code = vaddr;
        if ((vaddr + size) > mm->end_code)
        {
            mm->end_code = vaddr + size;
            mm->start_brk = mm->end_code;
            mm->brk = mm->start_brk;
        }       
    }

        

    mm->task_size += size;

//    debug("[UVM] Allocated mem reg va 0x%lX size 0x%lX status 0x%lX\n",
//            mreg->addr,mreg->size, mreg->status);
    release(&mm->lock);
    return SUCCESS;
}

mem_reg_t*
uvm_alloc_vmem(task_t *task, uint64_t vaddr, size_t size)
{
    list_head_t     *pos;
    mem_reg_t       *mreg, *mreg_stack = NULL;
    task_t          *t;
    uint64_t        a, ppn;
    mem_struct_t    *mm;
    pagetable_t     pgtable;
    bool            exist = false;


    // vaddr and size shoud be round up to page size 
    vaddr   = PGROUNDDOWN(vaddr);
    size    = PGROUNDUP(size);

    t           = task;    
    mm          = t->mm;
    pgtable     = mm->pagetable;

    acquire(&mm->lock);
//    debug("[UVM] Alloc mem reg task %d va 0x%lX size 0x%lX type 0x%lX\n",
//            t->pid,vaddr, size, type);
    if (vaddr == NULL){
        vaddr = __atomic_fetch_add(&mm->mmap_addr, size, __ATOMIC_ACQ_REL);
    } else {
        // search if addr is already allocated
        list_for_each(pos, &mm->memlist)
        {
            mreg = list_entry(pos, mem_reg_t, memlist);
            if((mreg->addr <= vaddr) && (vaddr < mreg->addr + mreg->size))
            {
                exist = true;
            }
        }  
        if (exist)
        {
            panic("[MMU] Memory region is allocated already");
            release(&mm->lock);
            return NULL;
        }    
    }

    mreg = (mem_reg_t *)malloc(sizeof(mem_reg_t));
    if(mreg == NULL){
        panic("[MMU] Can not allocate a mem block");
        release(&mm->lock);
        return NULL;
    }        

    mreg->addr = vaddr;
    mreg->size = size;
    mreg->status = 0;

    list_add_tail(&mm->memlist,&mreg->memlist);
    release(&mm->lock);
    return mreg;
}

static uint64_t flags2perm(uint64_t flags)
{
    uint64_t perm = 0;
    if (flags & MAP_WRITE)
        perm |= PTE_W;
    if (flags & MAP_EXEC)
        perm |= PTE_X;
    return perm;
}

mem_reg_t *uvm_user_memmap(task_t *task, uint64_t addr, uint64_t size, uint64_t flags, uint64_t paddr)
{
    pagetable_t pgtable;

    if (size == 0)
        return NULL;

    // ??? should it be default settings    
    if (flags == 0)
        return NULL;

    // Addressess should be page aligned
    if((addr % PAGE_SIZE != 0) || (paddr % PAGE_SIZE != 0)){
        return NULL;
    }

    if ((flags & MAP_MEMIO) && (paddr == NULL))
        return NULL;

    size = PGROUNDUP(size); 
    
    mem_reg_t *mreg = uvm_alloc_vmem(task, addr, size);
    pgtable = task->mm->pagetable;

    if (flags & MAP_MEMIO) {
        mreg->status |= (MM_REG_MMIO | MM_REG_VALID);
        if (mmu_map_pages(pgtable, mreg->addr, mreg->size, paddr, PTE_R | PTE_U | flags2perm(flags)) != SUCCESS){
            mfree(mreg);
            return 0;
        }            
    } else {
        // ??? Add private and shared blocks
        mreg->status |= (MM_REG_MEM | MM_REG_VALID);
        if (mmu_memmap(pgtable, mreg->addr, mreg->size, PTE_R | PTE_U | flags2perm(flags)) != SUCCESS) {
            mfree(mreg);
            return 0;
        }       
    }
  
    

//        debug("\x1b[35mPMPCFG0:\x1b[0m 0x%lX\n", r_pmpcfg0());
    return mreg;
}
