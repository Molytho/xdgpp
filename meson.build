project(
    'xdgpp',
    'cpp',
    version: '0.1',
    meson_version: '>= 1.9.0',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++20'],
)

sources = [
    'src/basedir.cpp',
    'src/desktop-entry.cpp',
    'src/desktop-entry/types.cpp',
    'src/desktop-entry/well-known-keys.cpp',
    'src/desktop-entry-parser.cpp'
]

includes = [include_directories('include')]

dependencies = [
    dependency('boost', modules: ['regex']),
]

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_LIBXDGPP']

lib = library(
    'xdgpp',
    sources,
    include_directories: includes,
    install: true,
    dependencies: dependencies,
    cpp_shared_args: lib_args,
    gnu_symbol_visibility: 'hidden',
)

test_exe = executable(
    'xdgpp_test',
    'test/xdgpp_test.cpp',
    include_directories: includes,
    link_with: lib,
)
test('xdgpp', test_exe)

# Make this library usable as a Meson subproject.
xdgpp_dep = declare_dependency(
    include_directories: includes,
    dependencies: dependencies,
    link_with: lib,
)
meson.override_dependency('xdgpp', xdgpp_dep)

# Make this library usable from the system's
# package manager.
install_headers(
    'include/desktop-entry.h',
    'include/basedir.h',
    'include/helper.h',
    subdir: 'xdgpp',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
    lib,
    description: 'A C++ library implementing the xdg basedir and xdg desktop-entry spec',
    filebase: 'xdgpp',
    name: 'xdgpp',
    subdirs: 'xdgpp',
    url: 'https://github.com/molytho/xdgpp',
    license: 'GPL-3.0-or-later'
)
