#include <common.h>
#include <mmu.h>
#include <memory.h>
#include <sched.h>
#include <errno.h>
#include <cap.h>

/*
 * Allocate memory region in given task.
 */
kerrno_t
uvm_alloc_mmreg(task_t *task, uint64_t vaddr, uint64_t size, uint64_t type, int xperm)
{
    list_head_t     *pos;
    mem_reg_t       *mreg, *mreg_stack = NULL;
    task_t          *t;
//    uint64_t        a, ppn;
    mem_struct_t    *mm;
    pagetable_t     pgtable;
    bool            exist = false;


    // vaddr and size shoud be round up to page size 
    vaddr   = PGROUNDDOWN(vaddr);
    size    = PGROUNDUP(size);

    t           = task;    
    mm          = t->mm;
    pgtable     = t->pagetable;

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

    mreg = (mem_reg_t *)ko_init((kobject_t *)mreg, t, KO_FRAME);

    if (cap_install(t, mreg, CAP_FRAME, CRIGHT_MAP)< 0) {
        mfree(mreg);
        panic("[UVM] can not allocate capability");
        return -ENOENT; 
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
    mem_reg_t       *mreg;
    task_t          *t;
//    uint64_t        a, ppn;
    mem_struct_t    *mm;
//    pagetable_t     pgtable;
    bool            exist = false;


    // vaddr and size shoud be round up to page size 
    vaddr   = PGROUNDDOWN(vaddr);
    size    = PGROUNDUP(size);

    t           = task;    
    mm          = t->mm;
//    pgtable     = mm->pagetable;

    acquire(&mm->lock);
//    debug("[UVM] Alloc mem reg task %d va 0x%lX size 0x%lX type 0x%lX\n",
//            t->pid,vaddr, size, type);
    if (!vaddr){
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

    mreg = (mem_reg_t *)ko_init((kobject_t *)mreg, t, KO_FRAME);

    if (cap_install(t, mreg, CAP_FRAME, CRIGHT_MAP)< 0) {
        mfree(mreg);
        panic("[UVM] can not allocate capability");
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

    if (!size || !flags)
        return NULL;

    if ((flags & MAP_MEMIO) && (!paddr))
        return NULL;

    // Addressess should be page aligned
    if((addr % PAGE_SIZE != 0) || (paddr % PAGE_SIZE != 0)){
        return NULL;
    }

    size = PGROUNDUP(size); 
    
    mem_reg_t *mreg = uvm_alloc_vmem(task, addr, size);
    pgtable = task->pagetable;

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

int uvm_init_mnode(task_t *t)
{
    uint64_t ppn = pgalloc();
    if (!ppn) return -ENOMEM;
    
    vmem_block_t *mnode = (vmem_block_t *)PPN2DA(ppn);
    memset(mnode, 0, PAGE_SIZE);

    cap_insert(&t->caps[MEM_ROOT], mnode, CAP_MNODE, 0);


    mnode[0].type = VMEM_UNTYPED;
    mnode[0].start = 0x1000;
    mnode[0].size = 0x1FFFFFFE;

    return SUCCESS;
}

vmem_block_t *uvm_find_free_slot(task_t *t)
{
    if (!t) return NULL;

    // cap root mem node
    vmem_block_t *mnode = (vmem_block_t *)t->caps[MEM_ROOT].obj;


// ??? FIXME add MNODE search and addind ne MNODES

    for (int i = 0; i < MAX_MEM_SLOTS; i++) {
        if (mnode[i].type == VMEM_NONE) {
//            mnode[i].type = VMEM_UNTYPED;
            return &mnode[i]; /* return slot pointer */
        }
    }
    return NULL; /* table full */
}

int uvm_alloc_vm(task_t *task, uint64_t vaddr, size_t size, uint16_t type, int xperm)
{
//    debug("[UVM] Task %d allocate vaddr 0x%lX size 0x%lX\n", task->pid, vaddr, size);

    vmem_block_t *mnode = (vmem_block_t *)task->caps[MEM_ROOT].obj;

    // round to page size block
    vaddr = PGROUNDDOWN(vaddr);
    size  = PGROUNDUP(size); 

    vmem_block_t *new;

    // lock ??? FIXME

    // if allocating vmem for code it shoud be from low addr to high

    if (type == VMEM_CODE) {
        // check if we have untyped mem
        if (mnode[0].type != VMEM_UNTYPED) panic("wrong mnode schema");

        // check if vaddr is in UNYTPED block
        if (mnode[0].start > vaddr) panic("vmem already allocated");

        // check if mem is available
        if ((size >> PAGE_SHIFT) > (mnode[0].size - ((vaddr - mnode[0].start) >> PAGE_SHIFT))) 
                panic("vmem no mem");

 //       debug("[UVM] Task %d UNTYPED start 0x%lX size 0x%lX\n", task->pid, mnode[0].start, mnode[0].size);

        uint32_t blks = ((vaddr - mnode[0].start) + size) >> PAGE_SHIFT;
 //       debug("[UVM] blocks shrinked 0x%lX\n", blks);
        mnode[0].size = mnode[0].size - blks;
        mnode[0].start = vaddr + size;      
 //       debug("[UVM] Task %d UNTYPED start 0x%lX size 0x%lX\n", task->pid, mnode[0].start, mnode[0].size);

        new = uvm_find_free_slot(task);

        new->start = vaddr;
        new->size  = size >> PAGE_SHIFT;
        new->type  = VMEM_CODE;
//        debug("[UVM] Task %d new block 0x%lX start 0x%lX size 0x%lX\n", task->pid, 
 //               new , new->start, new->size);

    }
    // unlock ??? FIXME


    return 0;
}


