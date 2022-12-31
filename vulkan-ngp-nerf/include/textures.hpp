#pragma once

#include "common_includes.h"
#include "types.hpp"
#include <vulkan/vulkan.hpp>

struct Texture {
  AllocatedImage image;
  vk::ImageView imageView;
  vk::Extent3D imageExtent;
  // this must be defined so that there is no copy constructor call
  Texture() = default;
  Texture(Texture &&) = default;
  Texture &operator=(Texture &&) = default;
  // Texture(const Texture &) = delete;
  // Texture &operator=(const Texture &) = delete;
  ~Texture() { spdlog::warn("texture destroyed!"); }
};