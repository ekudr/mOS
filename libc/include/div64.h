/*
 * Copyright (C) 2003 Bernardo Innocenti <bernie@develer.com>
 * Based on former asm-ppc/div64.h and asm-m68knommu/div64.h
 *
 * Optimization for constant divisors on 32-bit machines:
 * Copyright (C) 2006-2015 Nicolas Pitre
 *
 * The semantics of do_div() are:
 *
 * u32 do_div(u64 *n, u32 base)
 * {
 *	u32 remainder = *n % base;
 *	*n = *n / base;
 *	return remainder;
 * }
 *
 * NOTE: macro parameter n is evaluated multiple times,
 *       beware of side effects!
 */

#ifndef __DIV64_H__
#define __DIV64_H__

#include <stdint.h>

# define do_div(n,base) ({					\
	u32 __base = (base);				\
	u32 __rem;						\
	__rem = ((u64)(n)) % __base;			\
	(n) = ((u64)(n)) / __base;				\
	__rem;							\
 })

/* Wrapper for do_div(). Doesn't modify dividend and returns
 * the result, not remainder.
 */
static inline u64 lldiv(u64 dividend, u32 divisor)
{
	u64 __res = dividend;
	do_div(__res, divisor);
	return(__res);
}

#endif /* __DIV64_H__ */