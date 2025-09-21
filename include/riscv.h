#ifndef __RISCV_H__
#define __RISCV_H__

#include <stdint.h>

static inline uint8_t getreg8(const volatile uint64_t a)
{
  uint8_t v;
  __asm__ __volatile__("lb %0, 0(%1)" : "=r" (v) : "r" (a));
  return v;
}

static inline void putreg8(uint8_t v, const volatile uint64_t a)
{
  __asm__ __volatile__("sb %0, 0(%1)" : : "r" (v), "r" (a));
}

static inline uint16_t getreg16(const volatile uint64_t a)
{
  uint16_t v;
  __asm__ __volatile__("lh %0, 0(%1)" : "=r" (v) : "r" (a));
  return v;
}

static inline void putreg16(uint16_t v, const volatile uint64_t a)
{
  __asm__ __volatile__("sh %0, 0(%1)" : : "r" (v), "r" (a));
}

static inline uint32_t getreg32(const volatile uint64_t a) {
  uint32_t v;
  __asm__ __volatile__("lw %0, 0(%1)" : "=r" (v) : "r" (a));
  return v;
}

static inline void putreg32(uint32_t v, const volatile uint64_t a) {
  __asm__ __volatile__("sw %0, 0(%1)" : : "r" (v), "r" (a));
}

static inline uint64_t getreg64(const volatile uint64_t a)
{
  uint64_t v;
  __asm__ __volatile__("ld %0, 0(%1)" : "=r" (v) : "r" (a));
  return v;
}

static inline void putreg64(uint64_t v, const volatile uint64_t a)
{
  __asm__ __volatile__("sd %0, 0(%1)" : : "r" (v), "r" (a));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable


static inline uint64_t 
r_sstatus(void)
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, sstatus" : "=r" (x) );
    return x;
}

static inline void 
w_sstatus(uint64_t x)
{
    __asm__ __volatile__("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline uint64_t 
r_sip(void) 
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, sip" : "=r" (x) );
    return x;
}

static inline void 
w_sip(uint64_t x)
{
    __asm__ __volatile__("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

static inline uint64_t
r_sie(void)
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, sie" : "=r" (x) );
    return x;
}

static inline void 
w_sie(uint64_t x) 
{
    __asm__ __volatile__("csrw sie, %0" : : "r" (x));
}


// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void 
w_stvec(uint64_t x) 
{
    __asm__ __volatile__("csrw stvec, %0" : : "r" (x));
}

static inline uint64_t 
r_stvec(void) 
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, stvec" : "=r" (x) );
    return x;
}

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_sepc(uint64_t x) 
{
    __asm__ __volatile__("csrw sepc, %0" : : "r" (x));
}

static inline uint64_t 
r_sepc(void) 
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, sepc" : "=r" (x) );
    return x;
}

// Supervisor Trap Cause
static inline uint64_t 
r_scause(void) 
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, scause" : "=r" (x) );
    return x;
}

// Supervisor Trap Value
static inline uint64_t 
r_stval(void) 
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, stval" : "=r" (x) );
    return x;
}

// read and write tp, the thread pointer
static inline uint64_t 
r_tp(void) 
{
    uint64_t x;
    __asm__ __volatile__("mv %0, tp" : "=r" (x) );
    return x;
}

static inline void 
w_tp(uint64_t x) 
{
    __asm__ __volatile__("mv tp, %0" : : "r" (x));
}

static inline uint64_t 
r_sp(void) 
{
    uint64_t x;
    __asm__ __volatile__("mv %0, sp" : "=r" (x) );
    return x;
}

// supervisor-mode cycle counter
static inline uint64_t
r_time(void)
{
    uint64_t x;
    // asm volatile("csrr %0, time" : "=r" (x) );
    // this instruction will trap in SBI
    __asm__ __volatile__("rdtime %0" : "=r" (x) );
    return x;
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))
#define MAKE_USER_SATP(pagetable, asid) (SATP_SV39 | ((uint64_t)asid << 44) | (((uint64_t)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void 
w_satp(uint64_t x)
{
    __asm__ __volatile__("csrw satp, %0" : : "r" (x));
}

static inline uint64_t
r_satp(void)
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, satp" : "=r" (x) );
    return x;
}

// flush the TLB.
static inline void
sfence_vma(void)
{
    // the zero, zero means flush all TLB entries.
    __asm__ __volatile__("sfence.vma zero, zero");
}

// enable device interrupts
static inline void 
intr_on(void) 
{
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void 
intr_off(void) 
{
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int 
intr_get(void) 
{
    uint64_t x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

static inline uint64_t 
r_pmpcfg0(void)
{
    uint64_t x;
    __asm__ __volatile__("csrr %0, pmpcfg0" : "=r" (x) );
    return x;
}

#endif /* __RISCV_H__ */