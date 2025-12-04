project('KKdLib', 'cpp', version: '0.1', meson_version: '>=1.3')
warning_level = 3
cpp_std = 'c++26'
optimization = 3
b_lto = true
b_pgo = 'use'

cpp = meson.get_compiler('cpp')

add_project_arguments(
	cpp.get_supported_arguments(
		'-std=c++26',
		'-march=skylake',
		'-O3',
	),
	language: 'cpp',
)

add_project_link_arguments(
	cpp.get_supported_arguments(
		'-s',
		'-static-libgcc',
		'-static-libstdc++',
		'-Wl,-Bstatic',
		'-lstdc++',
		'-lpthread',
		'-Wl,-Bdynamic',
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
		'src/BC3.cpp',
		'src/BC5.cpp',
		'src/BC7.cpp',
		'src/main.cpp',
	],
	install: true,
	limited_api: '3.10'
)
