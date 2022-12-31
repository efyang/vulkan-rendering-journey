#define VMA_IMPLEMENTATION

#include <glm/common.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include <neural-graphics-primitives/testbed.h>
#include <neural-graphics-primitives/dlss.h>
#include <neural-graphics-primitives/common.h>
#include <neural-graphics-primitives/render_buffer.h>
#include <ngp.hpp>

#include "engine.hpp"

int main() {
  spdlog::info("Hello world!");
  // test whether ngp can be called into
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
