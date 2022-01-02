project('tutorial-engine', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

project_includes = include_directories('include')
subdir('shaders') # compiles all shaders to output/shaders
glfw = dependency('glfw3')
glm = dependency('glm')
vulkan = dependency('vulkan')

executable('tutorial-engine',
           'src/main.cpp',
           'src/window.cpp',
           'src/app.cpp',
           'src/pipeline.cpp',
           'src/device.cpp',
           'src/swapchain.cpp',
           include_directories : project_includes,
           dependencies: [glfw, vulkan, glm])
