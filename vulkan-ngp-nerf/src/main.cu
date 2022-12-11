#include <glm/common.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include <neural-graphics-primitives/testbed.h>
#include <neural-graphics-primitives/dlss.h>
#include <neural-graphics-primitives/common.h>
#include <neural-graphics-primitives/render_buffer.h>
#include <ngp.h>

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

  // ------------------------------ NGP test code -------------------------------------
  // initialize a testbed
  ngp::Testbed testbed(ngp::ETestbedMode::Nerf);
  // load the fox snapshot
  testbed.load_snapshot("../instant-ngp/data/nerf/fox/base.msgpack");
  
  // create an Eigen::vector
  Eigen::Vector4f rolling_shutter = Eigen::Vector4f::Zero();

  // create a vulkan texture
  ngp::VulkanTextureSurface vktex(Eigen::Vector2i(1920, 1080), 4);

  engine.run();
  engine.cleanup();

  spdlog::info("Shutdown complete");
  return 0;
}
