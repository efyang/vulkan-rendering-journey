project('triangle', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

project_includes = include_directories('include')
subdir('shaders') # compiles all shaders to output/shaders
glfw = dependency('glfw3')
vulkan = dependency('vulkan')

executable('triangle',
           'src/main.cpp',
           'src/window.cpp',
           'src/app.cpp',
           'src/pipeline.cpp',
           'src/device.cpp',
           include_directories : project_includes,
           dependencies: [glfw, vulkan])
