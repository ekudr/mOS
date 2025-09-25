#include <stdint.h>
#include <syscall.h>
#include <libsys/syscall.h>

uint64_t __fast_call_7(uint64_t n, uint64_t a, uint64_t b, uint64_t c, 
                            uint64_t d, uint64_t e, uint64_t f, uint64_t h,
                            uint64_t *_a, uint64_t *_b, uint64_t *_c, 
                            uint64_t *_d, uint64_t *_e, uint64_t *_f)
{
	register uint64_t a7 __asm__("a7") = n;
	register uint64_t a0 __asm__("a0") = a;
	register uint64_t a1 __asm__("a1") = b;
	register uint64_t a2 __asm__("a2") = c;
	register uint64_t a3 __asm__("a3") = d;
	register uint64_t a4 __asm__("a4") = e;
	register uint64_t a5 __asm__("a5") = f;
    register uint64_t a6 __asm__("a6") = h;
    __asm__ __volatile__ ("ecall\n\t" 
	            : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4), "+r"(a5), "+r"(a6)
                : "r"(a7), "0"(a0), "1"(a1), "2"(a2), "3"(a3), "4"(a4), "5"(a5), "6"(a6) 
                : "memory"); 
    *_a = a1; *_b = a2; *_c = a3; *_d = a4;
    *_e = a5; *_f = a6;

	return a0; 
}


