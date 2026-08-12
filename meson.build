project(
    'xdgpp',
    'cpp',
    version: '1.0.2',
    meson_version: '>= 1.9.0',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++20'],
)

molytho_cpp_utils_proj = subproject('molytho-cpp-utils')
molytho_cpp_utils_dep = molytho_cpp_utils_proj.get_variable('molytho_cpp_utils_dep')

subdir('lib')
subdir('utils')
