
kernel_sources = [
    'init.c',
    'mm/pgpool.c',
    'mm/mmu.c',
    'mm/kmem.c',
    'mm/shmem.c',
    'mm/vm.c',
    'mm/uvm.c',
    'trap.c',
    'sbi.c',
    'sched.c',
    'spinlock.c',
    'plic.c',
    'riscv_timer.c',
    'switch.S',
    'trampoline.S',
    'servers_loader.c',
    'syscall.c',
    'sysproc.c',
    'ipc.c',
    'irq.c',
    'khash.c',
    'signal.c',
]

kernel_a = static_library(
    'kernel.a',
    sources: kernel_sources,
    include_directories: [
        include_dir,
    ],
    c_args: c_args,
)