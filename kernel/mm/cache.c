#include <common.h>

#if defined(__SPACEMIT_K1__) 

#define RISCV_CBOM_BLOCK_SIZE   64

#define cbo_clean(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("mv a0, %0\n"    \
                             ".long 0x15200F\n"	\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})
/*
#define cbo_clean(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("cbo.clean"	\
							 " 0(%0)"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})
*/    

#define cbo_invalid(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__(" mv a0, %0\n"		\
                             ".long 0x5200F\n"  \
                             :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})
/*
#define cbo_invalid(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("cbo.inval"	\
							 " 0(%0)"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})
*//*
#define cbo_flush(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("mv a0, %0\n"	\
							 ".long 0x25200F\n"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})
*/
#define cbo_flush(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("cbo.flush"	\
							 " 0(%0)"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})

int check_cache_range(unsigned long start, unsigned long end)
{
	int ok = 1;

	if (start & (RISCV_CBOM_BLOCK_SIZE - 1))
		ok = 0;

	if (end & (RISCV_CBOM_BLOCK_SIZE - 1))
		ok = 0;

	if (!ok) {
		debug("CACHE: Misaligned operation at range [0x%08lx, 0x%08lx]\n",
			start, end);
	}

	return ok;
}

void flush_dcache_range(unsigned long start, unsigned long end)
{
    
	if (!check_cache_range(start, end))
		return;

	while (start < end) {
        // using virtual address
		cbo_flush(PA2DA(start));
		start += RISCV_CBOM_BLOCK_SIZE;
	}

}

void invalidate_dcache_range(unsigned long start, unsigned long end)
{
	if (!check_cache_range(start, end))
		return;

	while (start < end) {
		cbo_invalid(PA2DA(start));
		start += RISCV_CBOM_BLOCK_SIZE;
	}
}


void clean_dcache_range(unsigned long start, unsigned long end)
{
	if (!check_cache_range(start, end))
		return;

	while (start < end) {
		cbo_clean(PA2DA(start));
		start += RISCV_CBOM_BLOCK_SIZE;
	}
}

#endif

#if defined(__JH7110__)


#define STARFIVE_JH7110_L2CC_FLUSH_START 0x40000000UL
#define STARFIVE_JH7110_L2CC_FLUSH_SIZE 0x400000000UL

#define L2_CACHE_FLUSH64	0x200
#define L2_CACHE_BASE_ADDR 	0x2010000UL

#define CONFIG_SYS_CACHELINE_SIZE 64


#define RISCV_FENCE(p, s) \
	__asm__ __volatile__ ("fence " #p "," #s : : : "memory")

/* These barriers need to enforce ordering on both devices or memory. */
#define mb()		RISCV_FENCE(iorw,iorw)
#define rmb()		RISCV_FENCE(ir,ir)
#define wmb()		RISCV_FENCE(ow,ow)

void flush_dcache_range(unsigned long start, unsigned long end)
{
	unsigned long line;
	volatile unsigned long *flush64;

	/* make sure the address is in the range */
	if(start > end ||
		start < STARFIVE_JH7110_L2CC_FLUSH_START ||
		end > (STARFIVE_JH7110_L2CC_FLUSH_START +
				STARFIVE_JH7110_L2CC_FLUSH_SIZE))
		return;

	/*In order to improve the performance, change base addr to a fixed value*/
	flush64 = (volatile unsigned long *)PA2DA(L2_CACHE_BASE_ADDR + L2_CACHE_FLUSH64);

	/* memory barrier */
	mb();
	for (line = start; line < end; line += CONFIG_SYS_CACHELINE_SIZE) {
		(*flush64) = line;
		/* memory barrier */
		mb();
	}

	return;
}
#endif