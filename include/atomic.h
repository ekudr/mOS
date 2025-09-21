#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include <stdint.h>

static inline uint64_t
arch_atomic64_fetch_add(int i, uintptr_t a)
{
	register uint64_t ret;
    __asm__ __volatile__ (
		"amoadd.d.aqrl  %1, %2, %0\n"
		: "+A" (a), "=r" (ret)
		: "r" (i)
		: "memory");
	return ret;
}

static inline uint64_t
arch_atomic64_fetch_sub(int i, uintptr_t a)
{
	register uint64_t ret;
    __asm__ __volatile__ (
		"amoadd.d.aqrl  %1, %2, %0\n"
		: "+A" (a), "=r" (ret)
		: "r" (-i)
		: "memory");
	return ret;
}

#endif /* __ATOMIC_H__ */