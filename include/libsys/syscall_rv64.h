#include <stdint.h>

#define __asm_syscall(...) \
	__asm__ __volatile__ ("ecall\n\t" \
	: "+r"(a0) : __VA_ARGS__ : "memory"); \
	return a0; \

static inline uint64_t __syscall0(uint64_t n)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0");
	__asm_syscall("r"(a7))
}

static inline uint64_t __syscall1(uint64_t n, uint64_t a)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	__asm_syscall("r"(a7), "0"(a0))
}

static inline uint64_t __syscall2(uint64_t n, uint64_t a, uint64_t b)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	__asm_syscall("r"(a7), "0"(a0), "r"(a1))
}

static inline uint64_t __syscall3(uint64_t n, uint64_t a, uint64_t b, uint64_t c)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	register uint64_t a2 __asm__("a2") = c;
	__asm_syscall("r"(a7), "0"(a0), "r"(a1), "r"(a2))
}

static inline uint64_t __syscall4(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	register uint64_t a2 __asm__("a2") = c;
	register uint64_t a3 __asm__("a3") = d;
	__asm_syscall("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3))
}

static inline uint64_t __syscall5(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	register uint64_t a2 __asm__("a2") = c;
	register uint64_t a3 __asm__("a3") = d;
	register uint64_t a4 __asm__("a4") = e;
	__asm_syscall("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4))
}

static inline uint64_t __syscall6(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	register uint64_t a2 __asm__("a2") = c;
	register uint64_t a3 __asm__("a3") = d;
	register uint64_t a4 __asm__("a4") = e;
	register uint64_t a5 __asm__("a5") = f;
	__asm_syscall("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5))
}

static inline uint64_t __syscall7(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t h)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	register uint64_t a2 __asm__("a2") = c;
	register uint64_t a3 __asm__("a3") = d;
	register uint64_t a4 __asm__("a4") = e;
	register uint64_t a5 __asm__("a5") = f;
    register uint64_t a6 __asm__("a6") = h;
	__asm_syscall("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6))
}
