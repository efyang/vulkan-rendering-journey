#include "common_includes.h"

// glm
#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "engine.hpp"

namespace vkr {

void VulkanEngine::init_scene() {
  glm::vec3 cameraPosition = {0., -6., -10.};
  m_viewMatrix = glm::translate(glm::mat4(1.), cameraPosition);

  RenderObject monkey;
  monkey.mesh = get_mesh("monkey");
  monkey.material = get_material("defaultmesh");
  monkey.transformMatrix = glm::mat4(1.0f);
  m_renderables.push_back(monkey);

  RenderObject bunny;
  bunny.mesh = get_mesh("bunny");
  bunny.material = get_material("defaultmesh");
  bunny.transformMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(5, 0, 0));
  m_renderables.push_back(bunny);

  vk::SamplerCreateInfo samplerInfo;
  samplerInfo.setMagFilter(vk::Filter::eNearest);
  samplerInfo.setMinFilter(vk::Filter::eNearest);
  samplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
  samplerInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
  samplerInfo.setAddressModeW(vk::SamplerAddressMode::eRepeat);
  vk::Sampler blockySampler = m_device.createSampler(samplerInfo);
  m_mainDeletionQueue.push_function(
      [=] { m_device.destroySampler(blockySampler); });

  RenderObject empire;
  empire.mesh = get_mesh("empire");
  empire.material = get_material("texturedmesh");
  empire.transformMatrix =
      glm::translate(glm::mat4(1.0f), glm::vec3(5, -10, 0));

  // allocate descriptor set for single texture for material
  vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1,
                                          &m_singleTextureSetLayout);
  empire.material->textureSet = m_device.allocateDescriptorSets(allocInfo)[0];

  // point descriptor set to texture
  vk::DescriptorImageInfo imageBufferInfo(
      blockySampler, m_loadedTextures["empire_diffuse"].imageView,
      vk::ImageLayout::eShaderReadOnlyOptimal);
  vk::WriteDescriptorSet writeTex1;
  writeTex1.setImageInfo(imageBufferInfo);
  writeTex1.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
  writeTex1.setDstSet(empire.material->textureSet.value());
  writeTex1.setDstBinding(0);
  writeTex1.setDescriptorCount(1);
  m_device.updateDescriptorSets(writeTex1, nullptr);

  m_renderables.push_back(empire);

  for (int x = -20; x <= 20; x++) {
    for (int y = -20; y <= 20; y++) {
      RenderObject tri;
      tri.mesh = get_mesh("triangle");
      tri.material = get_material("defaultmesh");
      glm::mat4 translation =
          glm::translate(glm::mat4(1.0), glm::vec3(x, 0, y));
      glm::mat4 scale = glm::scale(glm::mat4(1.0), glm::vec3(0.2, 0.2, 0.2));
      tri.transformMatrix = translation * scale;

      m_renderables.push_back(tri);
    }
  }
}

} // namespace vkr