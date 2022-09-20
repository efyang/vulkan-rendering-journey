# from nyorain/vulkan-particles
project('vkguide-tutorial', 'cpp',
  version: '0.1.0',
  meson_version: '>=0.42',
  default_options: ['cpp_std=c++20'])

# default arrguments
# warnings and stuff
add_project_arguments(
	'-Wall',
	'-Wextra',
	'-Wno-unused-parameter',
	'-Wpedantic',
	'-Wno-missing-braces',
	'-Wno-missing-field-initializers',
	'-Wno-nullability-extension',
	'-Wno-nullability-completeness',
	language: 'cpp')

# project-specific stuff
source_root = meson.source_root().split('\\')
dep_vulkan = dependency('vulkan')
dep_sdl2 = dependency('sdl2')
dep_glm = dependency('glm')
dep_spdlog = dependency('spdlog')
dep_assimp = dependency('assimp')
dep_imgui = dependency('imgui')

subdir('assets/shaders')
main_inc = include_directories('include')
# shader_inc = include_directories('assets') # for headers in build folder

vkb_inc = include_directories('thirdparty/vk-bootstrap/src') # for headers in build folder
vma_inc = include_directories('thirdparty/VulkanMemoryAllocator-Hpp/VulkanMemoryAllocator/include') # for headers in build folder
vma_hpp_inc = include_directories('thirdparty/VulkanMemoryAllocator-Hpp/include') # for headers in build folder
stb_inc = include_directories('thirdparty/stb')
tinyobjloader_inc = include_directories('thirdparty/tinyobjloader')
imgui_inc = include_directories('subprojects/imgui-1.87')

src = [
	'src/pipeline.cpp',
	'src/main.cpp',
	'src/engine.cpp',
	'src/engine/input.cpp',
	'src/engine/init.cpp',
	'src/engine/material.cpp',
	'src/engine/shader.cpp',
	'src/engine/draw.cpp',
	'src/engine/scene.cpp',
	'src/engine/mesh.cpp',
	'src/mesh.cpp',
	'thirdparty/vk-bootstrap/src/VkBootstrap.cpp',
	]

executable('engine',
	src,
	dependencies: [
		dep_vulkan,
		dep_sdl2,
		dep_glm,
		dep_spdlog,
		dep_assimp,
		dep_imgui,
		],
	include_directories: [
		vkb_inc,
		stb_inc,
		vma_inc,
		vma_hpp_inc,
		tinyobjloader_inc,
		imgui_inc,
		main_inc,
		])
