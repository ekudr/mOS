#include <common.h>
#include <riscv.h>
#include <sched.h>
#include <memory.h>

struct {
    uintptr_t base;
    uintptr_t prio;
    uintptr_t sena;
    uintptr_t sprio;
    uintptr_t sclaim;
} plic;


static inline void 
plic_setpriority(uint64_t hartid, uint32_t p)
{
    putreg32(p, (uintptr_t)(PLIC_SPRIORITY(hartid) + kernel_map.pa_va_off));
}

static void 
__plic_toggle(uintptr_t enable_base, int irq, int enable) {
    uintptr_t reg = enable_base + (irq / 32) * 4;
    uint32_t irq_mask = 1 << (irq % 32);

    if (enable)
        putreg32(getreg32(reg) | irq_mask, reg);
    else 
        putreg32(getreg32(reg) & ~irq_mask, reg);
}

static inline uint32_t
__plic_claim(uint64_t hartid)
{
    return getreg32((uintptr_t)(PLIC_SCLAIM(hartid) + kernel_map.pa_va_off));
}

static inline void
__plic_complete(uint64_t hartid, uint32_t irq) 
{
    putreg32(irq, (uintptr_t)(PLIC_SCLAIM(hartid) + kernel_map.pa_va_off));
}

static void 
plic_toggle(uint64_t hartid, uint32_t irq, int enable) {

    // !!! lock enable regs

#ifdef __SPACEMIT_K1__
    if (!enable)
        __plic_claim(hartid);
#endif

    __plic_toggle(PLIC_SENABLE(hartid)  + kernel_map.pa_va_off, irq, enable);

#ifdef __SPACEMIT_K1__
    if (enable)
        __plic_claim(hartid);
#endif

    // !!! unlock enable regs
}

void 
plic_init(void) 
{

    plic.base = PLIC_BASE + kernel_map.pa_va_off;
    plic.prio = PLIC_PRIORITY + kernel_map.pa_va_off;
    plic.sclaim = PLIC_CLAIM + kernel_map.pa_va_off;

    debug("PLIC Base address 0x%lX, priority 0x%lX, claim 0x%lX\n", plic.base, plic.prio, plic.sclaim);
    
    // Priority 1 for all interrupts
    for (int id = 1; id <= NIRQS; id++)
        putreg32(1, (uintptr_t)(plic.prio + 4 * id));

    // Clean Interrupt pending map  
    for (int id = 0; id <= NIRQS / 8; id+=4)
        putreg32(0,(uintptr_t)(plic.base + 0x1000 + id));

    //  disable all interrupts for all cpus
    for (int c=0; c < NCPUS; c++)
        for (int id = 0; id <= NIRQS / 8; id+=4)
            putreg32(0, (uintptr_t)(PLIC_SENABLE(CPU2HARTID(c)) + (4 * (id / 32) + kernel_map.pa_va_off)));
        
    // set this hart's S-mode priority threshold to 7.
    for (int c=0; c < NCPUS; c++)
        plic_setpriority(c, 7);

}


// ask the PLIC what interrupt we should serve.
uint32_t 
plic_claim(void) 
{
    uint64_t hartid = current_cpu->hartid;
    return __plic_claim(hartid);
}

// tell the PLIC we've served this IRQ.
void 
plic_complete(uint32_t irq) 
{
    uint64_t hartid = current_cpu->hartid;
    __plic_complete(hartid, irq);
}

void plic_irq_enable(uint64_t hartid, uint32_t irq)
{
//    debug("[PLIC] enabling irq %d for hart %d\n", irq, hartid);

    plic_toggle(hartid, irq, 1);
}

void plic_irq_disable(uint64_t hartid, uint32_t irq)
{
//    debug("[PLIC] disabling irq %d for hart %d\n", irq, hartid);

    plic_toggle(hartid, irq, 0);
}

void plic_cpu_enable(uint64_t hartid)
{
    plic_setpriority(hartid, 0);
}

void plic_irq_complete(uint64_t hartid, uint32_t irq) 
{
//    debug("[PLIC] complete irq %d for hart %d\n", irq, hartid);
    __plic_complete(hartid, irq);
}