
libsys_sources = [
    'crt0.S',
    'ulib.c',
    'syscall.c',
#    'syscalls.S',
    'cap.c',
    'malloc.c',
    'printf.c',
#    'string.c',
    'debug.c',
    'riscv_timer.c',

]


libsys_a = static_library(
    'libsys.a',
    sources: libsys_sources,
    include_directories: [
        include_dir,
        include_libsys,
    ],
    c_args: c_args,
)