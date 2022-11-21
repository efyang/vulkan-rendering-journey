cmake_minimum_required(VERSION 3.18)

project(vulkan-ngp-nerf
    VERSION 1.0
    DESCRIPTION "Vulkan Renderer with integrated instant NGP"
	LANGUAGES C CXX CUDA
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_EXTENSIONS OFF)
set(CUDA_LINK_LIBRARIES_KEYWORD PUBLIC)
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)

list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-mf16c")
list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-Wno-float-conversion")
list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-fno-strict-aliasing")
list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-fPIC")

find_package(glm REQUIRED)
find_package(spdlog REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
add_subdirectory(thirdparty)

add_executable(engine
	src/pipeline.cpp
	src/main.cpp
	src/engine.cpp
	src/engine/input.cpp
	src/engine/init.cpp
	src/engine/material.cpp
	src/engine/shader.cpp
	src/engine/draw.cpp
	src/engine/scene.cpp
	src/engine/mesh.cpp
	src/mesh.cpp
	src/engine/textures.cpp
    src/main.cpp)

target_include_directories(engine PRIVATE
    include
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    )

target_link_libraries(engine PRIVATE
	spdlog::spdlog
	SDL2
	Vulkan::Vulkan
	glm
	vk-bootstrap::vk-bootstrap
	vma
	tinyobjloader
	stb-image)

add_subdirectory(assets)
add_dependencies(engine Shaders)