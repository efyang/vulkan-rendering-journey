#pragma once

#include "types.hpp"
#include <vulkan/vulkan.hpp>

struct Texture {
  AllocatedImage image;
  vk::ImageView imageView;
};