#include "common_includes.h"

#include <fstream>

#include "engine.hpp"

namespace vkr {

std::optional<vk::ShaderModule>
VulkanEngine::load_shader_module(const char *filePath) {
  // ate needed for tellg
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    spdlog::error("Shader file {} is already open!", filePath);
    return {};
  }
  size_t fsize = (size_t)file.tellg();
  std::vector<uint32_t> buffer((fsize + sizeof(uint32_t) - 1) /
                               sizeof(uint32_t));
  file.seekg(0);
  file.read((char *)buffer.data(), fsize);
  file.close();

  vk::ShaderModuleCreateInfo createInfo;
  createInfo.setCode(buffer);

  auto sm = m_device.createShaderModule(createInfo);
  // can be deleted right after pipeline is init, but I want to keep them
  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroyShaderModule(sm); });
  return sm;
}

void VulkanEngine::init_shader_modules() {
  m_shaderFiles = {
      {"fancytri.frag", "build/assets/shaders/fancytriangle.frag.spv"},
      {"trimesh.vert", "build/assets/shaders/tri_mesh.vert.spv"},
  };

  for (auto shader : m_shaderFiles) {
    auto mod = load_shader_module(shader.second.c_str());
    if (!mod.has_value()) {
      spdlog::error("Failed to load {} shader", shader.first);
      throw std::runtime_error("Cannot continue");
    } else {
      spdlog::info("Loaded {} shader successfully", shader.first);
      m_shaderModules[shader.first] = mod.value();
    }
  }
}

} // namespace vkr