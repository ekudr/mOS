#include <common.h>
#include <servers_loader.h>
#include <sched.h>
#include <spinlock.h>
#include <elf.h>
#include <mmu.h>
#include <memory.h>


int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}


/*
 * Load a program segment into pagetable at virtual address va.
 * va must be page-aligned
 * and the pages from va to va+sz must already be mapped.
 * Returns 0 on success, -1 on failure.
*/
static int
loadseg(pagetable_t pagetable, uint64_t va, uint64_t addr, uint64_t sz)
{
    uint64_t i, n, d;
    uint64_t pa, s;

    s = va % PAGE_SIZE;
    printf(".");
//    debug("EXEC read %d bytes to virt addr 0x%lX page shift %lX:\n", sz, va, s);
    if(s != 0) {
        pa = mmu_walk_addr(pagetable, va);
        if(pa == 0)
            panic("[LOADER] loadseg: user address should exist");
        pa += s;
        d = PAGE_SIZE - s;
        d = (sz < d) ? sz : d; 
        printf(".");
//        debug("    copy from 0x%lX to phis addr 0x%lX %d bytes\n", addr, pa, d); 
        memcpy((void *)PA2DA(pa), (void *)addr, d);
        addr += d;
        sz -= d;
    }

    for(i = 0; i < sz; i += PAGE_SIZE){
        pa = mmu_walk_addr(pagetable, va + i);
        if(pa == 0)
            panic("[LOADER] loadseg: user address should exist");
        
        if(sz - i < PAGE_SIZE)
            n = sz - i;
        else
            n = PAGE_SIZE;
        printf(".");    
//        debug("    copy from 0x%lX to phis addr 0x%lX %d bytes\n", addr+i, pa, n);    
        memcpy((void *)PA2DA(pa), (void *)(addr+i), n);
    }

    return 0;
}


/*
 * Create and execute first init task.
 */
int loader_execsvr(uint64_t addr)
{
    task_t  *t;
    elfhdr_t *elf;
    proghdr_t *ph;
    uint64_t sz = 0;

    t = sched_taskalloc();
    if (t == NULL)
        panic("[LOADER] cannot alloc task mem");

    acquire(&t->lock);

    // An empty user page table.
    t->pagetable = sched_task_pagetable(t);
    if (t->pagetable == 0)
    {
        sched_taskfree(t);
        return -1;
    }    

    t->mm->pagetable = t->pagetable;

    //Load the server sections

    elf = (elfhdr_t *)addr;
    if (elf->magic != ELF_MAGIC)
        panic("[LOADER]  elf load error");

    // Load program into user space.
    for (int i = 0, off = elf->phoff; i < elf->phnum; i++, off += sizeof(proghdr_t))
    {
        ph = (proghdr_t *)(addr + off);
        printf(".");        
//        debug("[LOADER] program type 0x%X, mem sz 0x%lX, file sz 0x%lX, Vaddr 0x%lX, Phis addr 0x%lX, file offset 0x%lX, allign 0x%lX\n",
//              ph->type, ph->memsz, ph->filesz, ph->vaddr, ph->paddr, ph->off, ph->align);
    
        if (ph->type != ELF_PROG_LOAD)
            continue;
        if (ph->memsz < ph->filesz)
            panic("init 01 load error");
        if (ph->vaddr + ph->memsz < ph->vaddr)
            panic("init 02 load error");
//        if (ph->vaddr % PAGESIZE != 0)
//            panic("init 03 load error");
        if (ph->memsz == 0)
            continue;
        uint64_t sz1;

        
        if (uvm_alloc_mmreg(t, ph->vaddr, ph->memsz, MM_REG_MEM, flags2perm(ph->flags)) < 0)
            panic("[LOADER] ERROR ALOCATING MEM_REG");
        sz1 = ph->vaddr + ph->memsz;
/*            
        if ((sz1 = mmu_user_vmalloc(t->pagetable, sz, ph->vaddr + ph->memsz, flags2perm(ph->flags))) == 0)
        {
            panic("[LOADER] mem alloc load error");
        }
 */            
        sz = (sz1);
        if (loadseg(t->pagetable, ph->vaddr, addr + ph->off, ph->filesz) < 0)
            panic("init 04 load error");  
          
    }    

    // Allocate some pages at the next page boundary.
    // Make the first inaccessible as a stack guard.
    // Use the rest as the user stack.
    sz = PGROUNDUP(sz);
/*
    uint64 sz1;
    if ((sz1 = mmu_user_vmalloc(t->pagetable, sz, sz + ( 1 + 1) * PAGE_SIZE, PTE_W)) == 0)
        panic("server load error");
    sz = sz1;    
    mmu_user_vmclear(t->pagetable, sz - ( 1 + 1) * PAGE_SIZE);

    //    stackbase = sz - 1*PAGESIZE;
*/
    t->sz = sz;

    if (uvm_alloc_mmreg(t, 0, 2 * PAGE_SIZE, MM_REG_STACK, PTE_W) < 0)
            panic("[LOADER] ERROR ALOCATING MEM_REG_STACK");

    /// ??? rewrite memory mapping for allocated buf
    mem_reg_t * mreg = uvm_user_memmap(t, 0, PAGE_SIZE, MAP_MEMIO | MAP_READ | MAP_WRITE, DA2PA(t->ipc_buf));

    t->trapframe->a0 = mreg->addr;
            
    t->trapframe->epc = elf->entry; // initial program counter = main
    t->trapframe->sp = t->mm->start_stack;   // initial stack pointer

//    debug("[LOADER] Task size is 0x%lX\n", t->mm->task_size);
    set_task_state(t, RUNNABLE);
    //  debug("[SCHED] new task allocated pid 0x%d state %d PT 0x%lX TrpFr 0x%lX\n", inittask->pid, inittask->state, inittask->pagetable, inittask->trapframe);
//      debug("[SCHED] context ra 0x%lX sp %lX ecp 0x%lX sp 0x%lX\n", t->context.ra, (uint64)t->context.sp, t->trapframe->epc, t->trapframe->sp);

    release(&t->lock);

    return 0;
}

void 
load_servers(void)
{
    printf("\nLoading system servers...\n");  
//    debug("Servers image starts at 0x%lX\n", _servers_img);

    svcshdr_t *hdr = (svcshdr_t *)(_uint64_t)_servers_img;
    svcsent_t *entry;

    

    if (hdr->magic != SVCSIMG_MAGIC)
    {
        panic("[LOADER] ERROR: wrong the services image\n");       
    }    
    
//    debug("[LOADER] %d servers to load:\n", hdr->nfiles);

    entry = (svcsent_t *)((uint64_t)_servers_img + sizeof(svcshdr_t));
//    debug("Servers entris starts at 0x%lX\n", entry);
    
    for(int n = 0; n < hdr->nfiles; n++){
//        if (strcmp(entry[n].name, "nameserver")) {
//            debug("This is it!!!!\n");
//        }
        printf("   %s ", entry[n].name);
//        debug("[LOADER] %d server %s %d bytes at 0x%lX\n", n, entry[n].name, 
//           entry[n].len, (uint64_t)_servers_img + entry[n].offset);
        loader_execsvr((uint64_t)_servers_img + entry[n].offset);
        printf("\n");
    }
    
}