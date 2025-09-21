

/* I supposed after U-boot it is S-mode */

    .section .text.enter , "ax"
    .global _start, _imglen

_start:
    j _kernel_start

    .balign 8
_imglen:
    .dword _end - _start

enable_mmu:
    la a1, kernel_map
    ld a1, 0(a1) 
    
    la a2, _start
    sub a1, a1, a2
    add ra, ra, a1

    la a2, 1f
    add a2, a2, a1
    csrw stvec, a2

    srl a2, a0, 0xc # PAGE_SHIFT
    li a1, 0x8000000000000000  # satp_mode Sv39
    or a2, a2, a1

    sfence.vma
    csrw satp, a2
.align 2
1:  
    /* Set trap vector to spin forever to help debug */
	la a0, .Lsecondary_park
	csrw stvec, a0


.option push
.option norelax
	la gp, __global_pointer$
.option pop

    ret


_kernel_start:

    /* Disable interrapts */
    csrw sie, zero
    csrw sip, zero

.option push
.option norelax
	la gp, __global_pointer$
.option pop

    la t1, boot_hartid
    sd a0, 0(t1)
    mv tp, a0

    /*  Set stack */
    la t0, _stack_top   /* Stack base */
    li t1, 4096     /* Stack size */
    mul t1, a0, t1  /* a0 is number oh HART */
    add t0, t0, t1  
    li t1, 4096     /* reserved stack */
    add t0, t0, t1
    mv sp, t0

    la a0, welcome
    call lib_puts
#    call write_str

    call early_vm_init

    la a0, early_pagetable
    ld a0, 0(a0)
    call enable_mmu

	la a0, kernelvec
	csrw stvec, a0

    /*  Set stack */
    la a0, boot_hartid
    ld a0, 0(a0)
    la t0, _stack_top   /* Stack base */
    li t1, 4096     /* Stack size */
    mul t1, a0, t1  /* a0 is HART id */
    add t0, t0, t1  
    li t1, 4096     /* reserved stack */
    add t0, t0, t1
    mv sp, t0

    call kernel_init
    tail kernel_init


_stop:
    wfi
    j _stop

# SBI print string
#    li a7, 0x4442434E
#    li a6, 0
#    li a0, 16
#    lla a1, welcome
#    li a2, 0
#    ecall


.align 2
.Lsecondary_park:
	/*
	 * Park this hart if we:
	 *  - have too many harts on CONFIG_RISCV_BOOT_SPINWAIT
	 *  - receive an early trap, before setup_trap_vector finished
	 *  - fail in smp_callin(), as a successful one wouldn't return
	 */

	wfi
	j .Lsecondary_park

.section .rodata
welcome: .string  "Welcome to mOS\r\n"


