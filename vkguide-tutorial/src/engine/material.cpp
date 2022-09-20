#include "common_includes.h"

#include "engine.hpp"

namespace vkr {

Material *VulkanEngine::create_material(VkPipeline pipeline,
                                        VkPipelineLayout layout,
                                        const std::string &name) {
  Material mat;
  mat.pipeline = pipeline;
  mat.pipelineLayout = layout;
  m_materials[name] = mat;
  return &m_materials[name];
}

Material *VulkanEngine::get_material(const std::string &name) {
  auto it = m_materials.find(name);
  if (it == m_materials.end()) {
    return nullptr;
  } else {
    return &(*it).second;
  }
}

} // namespace vkr