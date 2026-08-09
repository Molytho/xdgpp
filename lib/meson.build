lib_sources = [
    'src/basedir.cpp',
    'src/desktop-entry.cpp',
    'src/mime-apps.cpp',
    'src/shared-mime-info.cpp',
    'src/desktop-entry/types.cpp',
    'src/desktop-entry/well-known-keys.cpp',
    'src/desktop-entry-parser.cpp',
]

lib_includes = [include_directories('include')]

lib_dependencies = [
    dependency('boost', modules: ['regex']),
    dependency('xdgmime', fallback : ['xdgmime', 'libxdgmime_dep'])
]

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_LIBXDGPP']

lib = library(
    'xdgpp',
    lib_sources,
    include_directories: lib_includes,
    install: true,
    dependencies: lib_dependencies,
    cpp_shared_args: lib_args,
    gnu_symbol_visibility: 'hidden',
)

test_exe = executable(
    'xdgpp_test',
    'test/xdgpp_test.cpp',
    include_directories: lib_includes,
    link_with: lib,
)
test('xdgpp', test_exe)

# Make this library usable as a Meson subproject.
xdgpp_dep = declare_dependency(
    include_directories: lib_includes,
    dependencies: lib_dependencies,
    link_with: lib,
)
meson.override_dependency('xdgpp', xdgpp_dep)

# Make this library usable from the system's
# package manager.
install_headers(
    'include/desktop-entry/types.h',
    'include/desktop-entry/well-known-keys.h',
    'include/basedir.h',
    'include/desktop-entry.h',
    'include/mime-apps.h',
    'include/helper.h',
    subdir: 'xdgpp',
    preserve_path: true
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
    lib,
    description: 'A C++ library implementing the xdg basedir and xdg desktop-entry spec',
    filebase: 'xdgpp',
    name: 'xdgpp',
    subdirs: 'xdgpp',
    url: 'https://github.com/molytho/xdgpp',
    license: 'GPL-3.0-or-later',
)
