#include "common_includes.h"

// glm
#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "ngp.hpp"
#include "engine.hpp"

namespace vkr {

void VulkanEngine::init_scene() {
  // y axis is flipped
  // TODO: make this cleaner?
  glm::vec3 cameraPosition = {0., -2., -2.};
  m_viewMatrix = glm::translate(glm::mat4(1.), cameraPosition);

  RenderObject monkey;
  monkey.mesh = get_mesh("monkey");
  monkey.material = get_material("defaultmesh");
  monkey.transformMatrix = glm::mat4(1.0f);
  m_renderables.push_back(monkey);

  vk::SamplerCreateInfo samplerInfo;
  samplerInfo.setMagFilter(vk::Filter::eLinear);
  samplerInfo.setMinFilter(vk::Filter::eLinear);
  samplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
  samplerInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
  samplerInfo.setAddressModeW(vk::SamplerAddressMode::eRepeat);
  vk::Sampler blockySampler = m_device.createSampler(samplerInfo);
  m_mainDeletionQueue.push_function(
      [=] { m_device.destroySampler(blockySampler); });


  // nerf
  {
    // std::shared_ptr<NerfRenderObject> nerfRO = std::static_pointer_cast<NerfRenderObject>(nerfRO);
    auto nerfROptr = std::static_pointer_cast<NerfRenderObject>(nerfRO);
    // idk why this is necessary but it is i guess?
    RenderObject nerfro;
    // nerfROptr->renderObject->mesh = get_mesh("../instant-ngp/data/nerf/fox/base.msgpackaabbMesh");
    // nerfROptr->renderObject->material = get_material("defaultmesh");
    // nerfROptr->renderObject->transformMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-7, 0, 0));
    nerfro.mesh = get_mesh("../instant-ngp/data/nerf/fox/base.msgpackaabbMesh");
    nerfro.material = get_material("nerfmesh");
    glm::mat4 translate = glm::translate(glm::mat4(1.), glm::vec3(-4, 2, 0));
    // glm right-multiplies onto argument taken in
    glm::mat4 rotx = glm::rotate(glm::mat4(1.), 3.14f,
                                 glm::vec3(1., 0., 0.));
    glm::mat4 roty = glm::rotate(glm::mat4(1.), 0.0f,
                                 glm::vec3(0., 1., 0.));
    nerfro.transformMatrix = translate * roty * rotx;
    // nerfro.transformMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-1, 0, 0));
    // camera matrix needs to be relative to aabb center
    // something like this
    // nerfRO.cameraMatrix = m_viewMatrix 

    // allocate descriptor set for single texture for material
    vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1,
                                            &m_singleTextureSetLayout);
    nerfro.material->textureSet = m_device.allocateDescriptorSets(allocInfo)[0];

    // point descriptor set to texture
    vk::DescriptorImageInfo imageBufferInfo(
        blockySampler, m_loadedTextures["nerf"]->imageView,
        vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet writeTex1;
    writeTex1.setImageInfo(imageBufferInfo);
    writeTex1.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    writeTex1.setDstSet(nerfro.material->textureSet.value());
    writeTex1.setDstBinding(0);
    writeTex1.setDescriptorCount(1);
    m_device.updateDescriptorSets(writeTex1, nullptr);

    m_renderables.push_back(nerfro);
  }

  RenderObject bunny;
  bunny.mesh = get_mesh("bunny");
  bunny.material = get_material("defaultmesh");
  bunny.transformMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(5, 0, 0));
  m_renderables.push_back(bunny);

  {
    RenderObject empire;
    empire.mesh = get_mesh("empire");
    empire.material = get_material("texturedmesh");
    glm::mat4 translate = glm::translate(glm::mat4(1.), glm::vec3(0, 2, 0));
    // glm right-multiplies onto argument taken in
    glm::mat4 rotx = glm::rotate(glm::mat4(1.), glm::half_pi<float>(),
                                 glm::vec3(-1., 0., 0.));
    glm::mat4 roty = glm::rotate(glm::mat4(1.), glm::half_pi<float>(),
                                 glm::vec3(0., -1., 0.));
    empire.transformMatrix = translate * roty * rotx;

    // allocate descriptor set for single texture for material
    vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1,
                                            &m_singleTextureSetLayout);
    empire.material->textureSet = m_device.allocateDescriptorSets(allocInfo)[0];

    // point descriptor set to texture
    vk::DescriptorImageInfo imageBufferInfo(
        blockySampler, m_loadedTextures["empire_diffuse"]->imageView,
        vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet writeTex1;
    writeTex1.setImageInfo(imageBufferInfo);
    writeTex1.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    writeTex1.setDstSet(empire.material->textureSet.value());
    writeTex1.setDstBinding(0);
    writeTex1.setDescriptorCount(1);
    m_device.updateDescriptorSets(writeTex1, nullptr);

    m_renderables.push_back(empire);
  }

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

void VulkanEngine::update_scene() {
  auto nerfROptr = std::static_pointer_cast<NerfRenderObject>(nerfRO);
  glm::vec3 aabbShift(nerfROptr->bbCenter.x(), nerfROptr->bbCenter.y(), nerfROptr->bbCenter.z());
  // reapply both bbshift and object transform
  glm::mat4 translate = glm::translate(glm::mat4(1.), glm::vec3(-4, -2, 0));
  // glm right-multiplies onto argument taken in
  glm::mat4 rotx = glm::rotate(glm::mat4(1.), 3.14f,
                                glm::vec3(1., 0., 0.));
  // vulkan y is flipped?
  // can't figure y out
  glm::mat4 roty = glm::rotate(glm::mat4(1.), 0.0f,
                                glm::vec3(0., 1., 0.));
  glm::mat4 objectTransformMatrix = translate * roty * rotx;

  // reapply both bbshift and object transform to bring the camera into the same basis
  // as the object
  glm::mat4 correctedNerfCamMatrix =
    glm::translate(glm::mat4(1.0f), aabbShift)
    *objectTransformMatrix 
    * m_viewMatrix
    ;
  Eigen::Matrix<float, 3, 4> camera_matrix;
  // taken from basic init of instant-ngp testbed
  // camera_matrix << 1, 0, 0, 0.5,
  //       0, -1,  0, 0.5,
  //       0,  0, -1,   2;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 4; i++) {
      camera_matrix(j, i) = correctedNerfCamMatrix[i][j];
    }
  }
  camera_matrix(0, 0) *= -1;
  spdlog::info("eigen viewmat:");
  std::cout << camera_matrix << std::endl;
  nerfROptr->update(camera_matrix, *this);
}

} // namespace vkr