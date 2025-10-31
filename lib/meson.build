
lib_sources = [
    'string.c',
    'printf.c',
    'memset.S',
    'memmove.S',
    'memcpy.S',
    'kprint.c',
]

lib_a = static_library(
    'lib.a',
    sources: lib_sources,
    include_directories: [
        include_dir,
    ],
    c_args: c_args,
)