find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)

# add_library(tinyobjloader STATIC)
# target_include_directories(tinyobjloader PUBLIC tinyobjloader)
# target_sources(tinyobjloader PRIVATE
# 	tinyobjloader/tiny_obj_loader.h
# 	tinyobjloader/tiny_obj_loader.cc
# 	)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE
	VulkanMemoryAllocator-Hpp/include
	VulkanMemoryAllocator-Hpp/VulkanMemoryAllocator/include
	)


# add_library(stb-image INTERFACE)
# target_include_directories(stb-image INTERFACE
# 	stb)

add_subdirectory(vk-bootstrap)