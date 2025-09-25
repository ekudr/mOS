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

servers_img = custom_target('image',
    input: [nameserver_elf, devman_elf, tty_elf, one_elf],
    output: 'servers.img',
    command: [py3, meson.project_source_root() +'/tools/build_img.py', '@INPUT@', '-o', '@OUTPUT@'],
    depends: [devman_elf, one_elf],
    build_by_default: true
)

servers_dep = declare_dependency (sources: servers_img)