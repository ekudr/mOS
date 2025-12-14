#ifndef __FPU_H__
#define __FPU_H__

#include <riscv.h>

struct task;

static inline void set_fs_off(void)
{
    asm volatile("csrc sstatus, %0" :: "rK"(SSTATUS_FS));
}

static inline void set_fs_clean(void)
{
    asm volatile("csrs sstatus, %0" :: "rK"(SSTATUS_FS_CLEAN));
}

static inline void set_fs_initial(void)
{
    asm volatile("csrs sstatus, %0" :: "rK"(SSTATUS_FS_INITIAL));
}

static inline void set_fs_dirty(void)
{
    asm volatile("csrs sstatus, %0" :: "rK"(SSTATUS_FS_DIRTY));
}

static inline uint64_t read_sstatus_fs(void)
{
    return (r_sstatus() & SSTATUS_FS);
}

static inline void fpu_save_state(struct task *t)
{
    fpu_state_t *dst = t->fpu_state;

    set_fs_clean();

    asm volatile (
        "fsd f0,  0(%0)\n\t"
        "fsd f1,  8(%0)\n\t"
        "fsd f2,  16(%0)\n\t"
        "fsd f3,  24(%0)\n\t"
        "fsd f4,  32(%0)\n\t"
        "fsd f5,  40(%0)\n\t"
        "fsd f6,  48(%0)\n\t"
        "fsd f7,  56(%0)\n\t"
        "fsd f8,  64(%0)\n\t"
        "fsd f9,  72(%0)\n\t"
        "fsd f10, 80(%0)\n\t"
        "fsd f11, 88(%0)\n\t"
        "fsd f12, 96(%0)\n\t"
        "fsd f13, 104(%0)\n\t"
        "fsd f14, 112(%0)\n\t"
        "fsd f15, 120(%0)\n\t"
        "fsd f16, 128(%0)\n\t"
        "fsd f17, 136(%0)\n\t"
        "fsd f18, 144(%0)\n\t"
        "fsd f19, 152(%0)\n\t"
        "fsd f20, 160(%0)\n\t"
        "fsd f21, 168(%0)\n\t"
        "fsd f22, 176(%0)\n\t"
        "fsd f23, 184(%0)\n\t"
        "fsd f24, 192(%0)\n\t"
        "fsd f25, 200(%0)\n\t"
        "fsd f26, 208(%0)\n\t"
        "fsd f27, 216(%0)\n\t"
        "fsd f28, 224(%0)\n\t"
        "fsd f29, 232(%0)\n\t"
        "fsd f30, 240(%0)\n\t"
        "fsd f31, 248(%0)\n\t"
        :
        : "r"(&dst->f0)
        : "memory"

    );

    dst->fcsr = r_fcsr();
}

static inline void fpu_load_state(struct task *t)
{
    fpu_state_t *dst = t->fpu_state;

    set_fs_clean();

    asm volatile (
        "fld f0,  0(%0)\n\t"
        "fld f1,  8(%0)\n\t"
        "fld f2,  16(%0)\n\t"
        "fld f3,  24(%0)\n\t"
        "fld f4,  32(%0)\n\t"
        "fld f5,  40(%0)\n\t"
        "fld f6,  48(%0)\n\t"
        "fld f7,  56(%0)\n\t"
        "fld f8,  64(%0)\n\t"
        "fld f9,  72(%0)\n\t"
        "fld f10, 80(%0)\n\t"
        "fld f11, 88(%0)\n\t"
        "fld f12, 96(%0)\n\t"
        "fld f13, 104(%0)\n\t"
        "fld f14, 112(%0)\n\t"
        "fld f15, 120(%0)\n\t"
        "fld f16, 128(%0)\n\t"
        "fld f17, 136(%0)\n\t"
        "fld f18, 144(%0)\n\t"
        "fld f19, 152(%0)\n\t"
        "fld f20, 160(%0)\n\t"
        "fld f21, 168(%0)\n\t"
        "fld f22, 176(%0)\n\t"
        "fld f23, 184(%0)\n\t"
        "fld f24, 192(%0)\n\t"
        "fld f25, 200(%0)\n\t"
        "fld f26, 208(%0)\n\t"
        "fld f27, 216(%0)\n\t"
        "fld f28, 224(%0)\n\t"
        "fld f29, 232(%0)\n\t"
        "fld f30, 240(%0)\n\t"
        "fld f31, 248(%0)\n\t"
        :
        : "r"(&dst->f0)
        : "memory"

    );

    w_fcsr(dst->fcsr);
}


#endif /* __FPU_H__ */