#include <glm/common.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include "engine.hpp"

int main() {
  spdlog::info("Hello world!");

  vkr::VkEngine engine;
  engine.init();
  engine.run();
  engine.cleanup();

  spdlog::info("Shutdown complete");
  return 0;
}
