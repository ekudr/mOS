    .section .text, "ax"
    .global __thread_trampoline

// Child entry point set by thread_spawn via TASK_OP_RUN.
// On entry: a0 = VA of parent's ipc_buf mapped into child (set by cap_task_create).
// Initializes __ipc_buffer and GP, then jumps to __thread_main.
__thread_trampoline:
    .option push
    .option norelax
    lla gp, __global_pointer$
    .option pop

    la  t0, __ipc_buffer
    sd  a0, 0(t0)

    call __thread_main
    call exit
    tail exit
