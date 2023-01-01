#pragma once

#include "engine.hpp"
#include "neural-graphics-primitives/testbed.h"
#include <mesh.hpp>
#include <string>
#include <textures.hpp>

namespace ngp {
bool ends_with(const std::string &str, const std::string &suffix);
}

namespace vkr {
struct NerfRenderObject {
  // mesh is cube mesh, instantiated with aabb dimensions
  // a map will only hold references - we still need to have actual storage
  // backing the mesh.
  std::shared_ptr<Mesh> aabbMesh;
  std::shared_ptr<RenderObject> renderObject;
  // buffer of textures to render into - just 1 for now to kiss
  std::shared_ptr<Texture> renderTexture;
  // raw vulkan texture accessible from ngp
  std::shared_ptr<ngp::VulkanTextureSurface> rawVkImage;
  // references rawVkTexture, used by testbed to render into
  std::unique_ptr<ngp::CudaRenderBuffer> cudaRenderBuffer;

  // Testbed per object - this is technically really bad but we only really care
  // about rendering one for now
  ngp::Testbed testbed = ngp::Testbed(ngp::ETestbedMode::Nerf);
  int texWidth;
  int texHeight;
  // updates the texture before this renderobject is used
  NerfRenderObject() = delete;
  NerfRenderObject(int texWidth, int texHeight, const std::string snapshotPath,
                   VulkanEngine &engine);
  ~NerfRenderObject();
  Eigen::Matrix<float, 3, 4> prevCameraMatrix;
  Eigen::Matrix<float, 3, 4> cameraMatrix;
  Eigen::Vector3f bbCenter;
  void update(Eigen::Matrix<float, 3, 4> camera_matrix, VulkanEngine &engine);
};
} // namespace vkr