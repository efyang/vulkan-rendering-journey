#include <glm/common.hpp>
#include <iostream>
#include <neural-graphics-primitives/ngp.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include "engine.hpp"

int main() {
  spdlog::info("Hello world!");
  if (ngp::ends_with("wow this is crazy", "crazy") &&
      !ngp::ends_with("that this works", "test")) {
    spdlog::info("NGP linked properly!");
  }
  vkr::VulkanEngine engine;
  engine.init();
  engine.run();
  engine.cleanup();

  spdlog::info("Shutdown complete");
  return 0;
}
