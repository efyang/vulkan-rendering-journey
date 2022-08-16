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
	language: 'cpp')

# project-specific stuff
source_root = meson.source_root().split('\\')
dep_vulkan = dependency('vulkan')
dep_sdl2 = dependency('sdl2')
dep_glm = dependency('glm')
dep_spdlog = dependency('spdlog')

subdir('assets/shaders')
# shader_inc = include_directories('assets') # for headers in build folder

vkb_inc = include_directories('thirdparty/vk-bootstrap/src') # for headers in build folder
vma_inc = include_directories('thirdparty/VulkanMemoryAllocator/include') # for headers in build folder
stb_inc = include_directories('thirdparty/stb')

src = [
	'src/pipeline.cpp',
	'src/main.cpp',
	'src/engine.cpp',
	'thirdparty/vk-bootstrap/src/VkBootstrap.cpp',
	]

executable('engine', src, dependencies: [dep_vulkan, dep_sdl2, dep_glm, dep_spdlog], include_directories: [vkb_inc, vma_inc, stb_inc])
