

    .global sbi_putc, write_str, panic, _putchar
_putchar:
sbi_putc:
#    push ra
#    li a1, 0
#    li a2, 0
 #   li a3, 0
#    li a4, 0
#    li a5, 0
    li a6, 0
    li a7, 1
    ecall
#    pop ra
    ret

write_str:
#    push ra
    mv t1, a0
_write_loop:
    lb a0, 0(t1)
	beqz a0, _write_end
    call sbi_putc
    li t2, 1
    add t1, t2, t1
    j _write_loop
_write_end:
#    pop ra
    ret

panic:
    call lib_puts
_stop:
    wfi
    j _stop