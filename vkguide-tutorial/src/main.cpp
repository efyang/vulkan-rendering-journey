#include <glm/common.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC
#include <vulkan/vulkan.hpp>

#include "engine.hpp"

int main() {
  spdlog::info("Hello world!");

  vkr::VulkanEngine engine;
  engine.init();
  engine.run();
  engine.cleanup();

  spdlog::info("Shutdown complete");
  return 0;
}
