#pragma once

#include "common_includes.h"
#include <vulkan/vulkan.hpp>

struct AllocatedBuffer {
  vk::Buffer buffer;
  vma::Allocation allocation;
};

struct AllocatedImage {
  vk::Image image;
  vma::Allocation allocation;
};