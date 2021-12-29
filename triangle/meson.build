project('triangle', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

project_includes = include_directories('include')
glfw = dependency('glfw3')

executable('triangle',
           'src/main.cpp',
           'src/window.cpp',
           'src/app.cpp',
           include_directories : project_includes,
           dependencies: [glfw])
