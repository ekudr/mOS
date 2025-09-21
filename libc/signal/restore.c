

void __restore()
{
    __asm__ __volatile__(
        "li a7, 42 # SYS_sig_ret"
	    "ecall"
    );
}