linker_script_flags = '-T' + meson.current_source_dir()/'servers.lds'

linker_args = [
        '-nostdlib',
        '-static',
        '-O3',
#        linker_script_flags,
]

nameserver_elf = executable(
    'nameserver',
    sources : ['nameserver/main.c'],
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

vfs_elf = executable(
    'vfs',
    sources : ['vfs/vfs.c'],
    include_directories: [include_libc, include_board, include_libsys],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

devman_elf = executable(
    'devman',
    sources : ['devman.c'],
    include_directories: [include_libc, include_board, include_libsys],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

one_elf = executable(
    'one',
    sources : ['one.c'],
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

tty_elf = executable(
    'tty',
    sources : ['tty.c'],
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],

)

console_elf = executable(
    'console',
    sources : ['console/main.c', 'console/text_render.c',],
    objects : ['console/font.o'],
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],

)

wm_elf = executable(
    'wm',
    sources : ['wm/main.c', 'wm/window.c', 'wm/draw.c', ],
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

mmc_sources = [
    'mmc_drv/main.c', 
    'mmc_drv/mmc.c',
    'mmc_drv/device.c',
]

if board.contains('vf2')
    mmc_sources += ['mmc_drv/dwmmc.c']
endif
if board.contains('qemu')
    
endif
if board.contains('bpi-f3')
    mmc_sources += ['mmc_drv/sdhci-k1.c', 'mmc_drv/sdhci.c']
endif

mmc_drv_elf = executable(
    'mmc_drv',
    sources : mmc_sources,
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],

)

usb2_sources = [
    'usb2/main.c', 
    'usb2/ehci.c',
    'usb2/phy.c',
    'usb2/clock-k1x.c',
]

usb2_elf = executable(
    'usb2',
    sources : usb2_sources,
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

usb3_sources = [
    'usb3/main.c', 
    'usb3/dwc3.c',
    'usb3/phy.c',
    'usb3/clock-k1x.c',
]

usb3_elf = executable(
    'usb3',
    sources : usb3_sources,
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

asd_elf = executable(
    'asd',
    sources : ['asd.c'],
    include_directories: [include_libc, include_board, include_libsys, ],
    c_args: c_args,
    link_args: [linker_args],
    link_with: [libsys_a, libc_a, ],
    link_depends: [libsys_a, libc_a, ],
)

servers_img = custom_target('image',
    input: [nameserver_elf, vfs_elf, devman_elf, tty_elf, usb3_elf,
             mmc_drv_elf, wm_elf, console_elf, one_elf],
    output: 'servers.img',
    command: [py3, meson.project_source_root() +'/tools/build_img.py', '@INPUT@', '-o', '@OUTPUT@'],
    depends: [devman_elf, one_elf],
    build_by_default: true
)

servers_dep = declare_dependency (sources: servers_img)