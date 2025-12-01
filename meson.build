project('KKdLib', 'cpp', version: '0.1')
warning_level = 3
cpp_std = 'c++17'
optimization = 3
b_lto = true
b_pgo = 'use'

cpp = meson.get_compiler('cpp')

add_project_arguments(
	cpp.get_supported_arguments(
		'-DPy_LIMITED_API=0x030A0000',
		'-std=c++26',
		'-march=skylake',
		'-O3',
	),
	language: 'cpp',
)

add_project_link_arguments(
	cpp.get_supported_arguments(
		'-s',
		'-std=c++26',
		'-O3',
	),
	language: 'cpp',
)

kkdlib = subproject('KKdLib')

py = import('python').find_installation(pure: false)

py.extension_module(
	'KKdLib',
	dependencies : [
		kkdlib.get_variable('KKdLib_dep'),
	],
	cpp_pch: 'src/helpers.h',
	sources : [
		'src/main.cpp',
	]
)
