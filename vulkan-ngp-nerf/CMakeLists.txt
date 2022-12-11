cmake_minimum_required(VERSION 3.18)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CPP_COMPILER clang++)

enable_language(CUDA)
project(vulkan-ngp-nerf
    VERSION 1.0
    DESCRIPTION "Vulkan Renderer with integrated instant NGP"
	LANGUAGES C CXX CUDA
)

# ccache build optimization
find_program(CCACHE "ccache")
if(CCACHE)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# need to match instant ngp cuda arch
# https://github.com/NVlabs/instant-ngp/issues/194
# set(CMAKE_CUDA_ARCHITECTURES ${TCNN_CUDA_ARCHITECTURES})
set(CMAKE_CUDA_ARCHITECTURES 86)
list(APPEND CUDA_NVCC_FLAGS "-gencode=arch=compute_86,code=\"sm_86,compute_86\"")

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_EXTENSIONS OFF)
set(CUDA_LINK_LIBRARIES_KEYWORD PUBLIC)
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)

list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-mf16c")
list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-Wno-float-conversion")
list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-fno-strict-aliasing")
list(APPEND CUDA_NVCC_FLAGS "-Xcompiler=-fPIC")
list(APPEND CUDA_NVCC_FLAGS "--extended-lambda")
list(APPEND CUDA_NVCC_FLAGS "--expt-relaxed-constexpr")

add_subdirectory(instant-ngp)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(CUDA REQUIRED)
add_subdirectory(thirdparty)

add_executable(engine
	src/pipeline.cpp
	src/main.cu
	src/engine.cpp
	src/engine/input.cpp
	src/engine/init.cu
	src/engine/material.cpp
	src/engine/shader.cpp
	src/engine/draw.cpp
	src/engine/scene.cpp
	src/engine/mesh.cpp
	src/mesh.cpp
	src/engine/textures.cpp
    )

target_include_directories(engine PRIVATE
    include
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
	instant-ngp/include
	${NGP_INCLUDE_DIRECTORIES}
	${CUDA_INCLUDE_DIRS}
    )

target_link_libraries(engine PRIVATE
	spdlog::spdlog
	SDL2
	Vulkan::Vulkan
	glm
	vk-bootstrap::vk-bootstrap
	vma
	# tinyobjloader
	# stb-image
	ngp
	)

add_subdirectory(assets)
add_dependencies(engine Shaders)