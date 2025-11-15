

    .section .text ,"ax"
    .global _start
_start:

    

    .option push
    .option norelax
    lla gp, __global_pointer$
    .option pop

    mv s0, a0
    la t0, __ipc_buffer
    sd s0, 0(t0)

    call main

    call exit
    tail exit


