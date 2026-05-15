/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_FENCE_H
#define _ASM_FENCE_H

#define RISCV_FENCE_ASM(p, s)		"\tfence " #p "," #s "\n"
#define RISCV_FENCE(p, s) \
	({ __asm__ __volatile__ (RISCV_FENCE_ASM(p, s) : : : "memory"); })


#define RISCV_ACQUIRE_BARRIER		RISCV_FENCE_ASM(r, rw)
#define RISCV_RELEASE_BARRIER		RISCV_FENCE_ASM(rw, w)
#define RISCV_FULL_BARRIER		RISCV_FENCE_ASM(rw, rw)


#endif	/* _ASM_RISCV_FENCE_H */