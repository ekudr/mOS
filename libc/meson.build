include_libc = [include_directories('./include')]

c_args_libc = [
    '-nostdinc',
    '-pipe',
    '-ffreestanding',
    '-O3',
#    '-Wno-implicit-int',
#    '-Wno-int-conversion',
#    '-Wno-implicit-function-declaration',
#    '-Wno-declaration-missing-parameter-type',
]

if board.contains('vf2')
    c_args_libc += '-mcpu=sifive-u74'
    c_args_libc += '-march=rv64imafdc_zba_zbb_zicsr_zifencei'
endif
if board.contains('qemu')
    c_args_libc += '-mcpu=sifive-u54'
    c_args_libc += '-march=rv64imafdc_zicsr_zifencei'
endif

if board.contains('bpi-f3')
#    c_args_libc += '-mcpu=spacemit-k1'
#    c_args_libc += '-mcpu=spacemit-x60'
    c_args_libc += '-march=rv64imafdc_zba_zbb_zbc_zbs_zicsr_zifencei_zicbom'
endif

libc_string = [
    'string/memchr.c',
    'string/memcmp.c',
    'string/memcpy.S',
    'string/memmem.c',
    'string/memmove.S',
    'string/memset.S',
    'string/memrchr.c',
    'string/strchrnul.c',
    'string/strchr.c',
    'string/strcmp.c',
    'string/stpcpy.c',
    'string/strcpy.c',
    'string/strncpy.c',
    'string/strrchr.c',
    'string/strlen.c',
    'string/ffs.S',
]

libc_mosstd = [
    'mosstd/getpid.c',
    'mosstd/ipc.c',
    'mosstd/devman.c',
    'mosstd/nameserver.c',
    'mosstd/vfs.c',
    'mosstd/console.c',
    'mosstd/block_dev.c',
    'mosstd/ext4.c',
]

libc_malloc = [
    
]

libc_signal = [
    'signal/sigaction.c',
    'signal/restore.c',
]

libc_a = static_library(
    'libc.a',
    sources: [
        libc_string,
        libc_malloc,
        libc_mosstd,
        libc_signal, 
    ],
    include_directories: [
        include_libc,
    ],
    c_args: c_args_libc,
)