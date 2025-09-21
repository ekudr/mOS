

    .section .text ,"ax"
    .global _start
_start:

    .option push
    .option norelax

    lla gp, __global_pointer$
    .option pop

    call main

    call exit
    tail exit


