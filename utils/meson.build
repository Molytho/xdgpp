common_sources = [
    'common/spawn_helper.cpp'
]

common_include = [
    include_directories('common')
]

common_lib = static_library(
    'utils_common_lib',
    common_sources,
    include_directories: common_include,
    install: false
)

open_sources = [
    'open/main.cpp',
]

open_include = common_include

open_dependencies = [
    common_lib,
    dependency('boost', modules: ['regex']),
]

executable(
    'xdgpp_open',
    open_sources,
    include_directories: lib_includes + open_include,
    link_with: [common_lib, lib],
)

launch_sources = [
    'launch/main.cpp',
]

launch_include = common_include

executable(
    'xdgpp_launch',
    launch_sources,
    include_directories: lib_includes + launch_include,
    link_with: [common_lib, lib],
)

