project(
    'mOS',
    ['c'],
    version : '0.0.1',
    meson_version : '>= 1.1.0',
)

cc = meson.get_compiler('c')

objcopy = find_program('objcopy', required: true)
py3 = find_program('python3', required: true)
has_mtune = cc.has_argument('-mtune')
has_mcpu = cc.has_argument('-mcpu')
has_march = cc.has_argument('-march')

c_args = [
    '-nostdlib',
#    '-fno-stack-protector',
    '-static', 
    '-mcmodel=medany',   
#    '-mno-relax',
    '-ffreestanding',
    '-O3',
]

buildtype = get_option('buildtype')
if buildtype.contains('debug')
    c_args += '-D__DEBUG__'
endif

board = get_option('board')

subdir('include')

if board.contains('vf2')
    c_args += '-mcpu=sifive-u74'
    c_args += '-march=rv64imafdc_zba_zbb_zicsr_zifencei'
endif
if board.contains('qemu')
    c_args += '-mcpu=sifive-u54'
    c_args += '-march=rv64imafdc_zicsr_zifencei'
endif

if board.contains('bpi-f3')
    
#    c_args += '-mcpu=spacemit-k1'
#    c_args += '-mcpu=spacemit-x60'
    c_args += '-march=rv64imafdc_zba_zbb_zbc_zbs_zicsr_zifencei_zicbom'
endif

subdir('board')
include_dir += include_board

subdir('lib')
subdir('kernel')
subdir('libsys')
subdir('libc')
subdir('servers')



mos_sources = [
    'start.S',
    'kernelvec.S',
    'sbi_print.S',
    'mm_init.c',
    'servers_img.S',
]

linker_script_flags = '-T' + meson.current_source_dir()/'boot.lds'

mos_elf = executable(
    'mos.elf',
    sources : mos_sources,
    include_directories: [include_dir],
    c_args: c_args,
    dependencies: [servers_dep,],
    link_args: [
        '-nostdlib',
        '-static',
        linker_script_flags,
    ],
        link_with: [
        kernel_a,
        lib_a
    ],
    link_depends: [
        kernel_a,
        lib_a,
    ],
)

image = custom_target('image',
    input: mos_elf,
    output: 'img.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    depends: mos_elf,
    build_by_default: true
)

summary(
  {
    'Built type': get_option('buildtype'),
  },
  section: 'Build settings'
)

summary(
  {
    'Board': get_option('board'),
  },
  section: 'Platform'
)