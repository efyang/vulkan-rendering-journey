#pragma once

#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

struct AllocatedBuffer {
  vk::Buffer buffer;
  vma::Allocation allocation;
};

struct AllocatedImage {
  vk::Image image;
  vma::Allocation allocation;
};