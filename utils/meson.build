common_sources = ['common/spawn_helper.cpp', 'common/uri_helper.cpp']

common_include = [include_directories('common')]

common_dependencies = [molytho_cpp_utils_dep, xdgpp_dep]

common_lib = static_library(
    'utils_common_lib',
    common_sources,
    dependencies: common_dependencies,
    include_directories: common_include,
    install: false,
)

common_dep = declare_dependency(
    include_directories: common_include,
    dependencies: common_dependencies,
    link_with: common_lib,
)

open_sources = [
    'open/main.cpp',
]

open_include = []

open_dependencies = [
    common_dep,
    dependency('boost', modules: ['regex']),
]

executable(
    'xdgpp_open',
    open_sources,
    dependencies: open_dependencies,
    include_directories: open_include,
)

launch_sources = [
    'launch/main.cpp',
]

launch_include = []

launch_dependencies = [
    common_dep,
]

executable(
    'xdgpp_launch',
    launch_sources,
    dependencies: launch_dependencies,
    include_directories: launch_include,
)
