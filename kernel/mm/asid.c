#include <common.h>
#include <spinlock.h>
#include <mmu.h>
#include <memory.h>

static struct {
    spinlock_t  lock;
    uint16_t    next_asid;
    uint64_t    cur_gen;
    uint16_t    asid_max;
} asid_state;

void asid_init(void)
{
    initlock(&asid_state.lock, "asid");
    asid_state.cur_gen  = 1;
    asid_state.next_asid = 1;
    asid_state.asid_max = (uint16_t)(kernel_map.asid_max ? kernel_map.asid_max : 0xFFFF);
}

/*
 * Allocate an ASID for mm. Uses a rolling counter with generation epochs.
 * When the counter wraps, bump the generation and flush all TLBs globally
 * so stale ASID-tagged entries are invalidated before reuse.
 * Returns the assigned ASID (also stored in mm->asid / mm->asid_gen).
 */
uint16_t asid_alloc(mem_struct_t *mm)
{
    acquire(&asid_state.lock);

    if (mm->asid != 0 && mm->asid_gen == asid_state.cur_gen) {
        uint16_t asid = mm->asid;
        release(&asid_state.lock);
        return asid;
    }

    uint16_t asid = asid_state.next_asid++;
    if (asid_state.next_asid > asid_state.asid_max || asid_state.next_asid == 0) {
        asid_state.next_asid = 1;
        asid_state.cur_gen++;
        // Global TLB flush: invalidate all ASID-tagged entries on all harts.
        sbi_remote_sfence_vma(0, (uint64_t)-1);
        asid = asid_state.next_asid++;
    }

    mm->asid     = asid;
    mm->asid_gen = asid_state.cur_gen;

    release(&asid_state.lock);
    return asid;
}

// No-op: generation epoch handles ASID reclaim automatically.
void asid_free(mem_struct_t *mm)
{
    mm->asid     = 0;
    mm->asid_gen = 0;
}
