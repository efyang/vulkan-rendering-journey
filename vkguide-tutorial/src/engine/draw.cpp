#include "common_includes.h"

// glm
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <algorithm>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "constants.h"
#include "engine.hpp"

namespace vkr {

const FrameData &VulkanEngine::get_current_frame() {
  return m_frames[m_frameNumber % FRAME_OVERLAP];
}

void VulkanEngine::draw() {
  // timeout in ns
  (void)m_device.waitForFences(get_current_frame().m_renderFence, true,
                               S_TO_NS);
  m_device.resetFences(get_current_frame().m_renderFence);

  // request swapchain image
  uint32_t swapchainImageIndex =
      m_device
          .acquireNextImageKHR(m_swapchain, S_TO_NS,
                               get_current_frame().m_presentSemaphore, nullptr)
          .value;

  get_current_frame().m_mainCommandBuffer.reset();
  vk::CommandBufferBeginInfo cmdBeginInfo;
  cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  get_current_frame().m_mainCommandBuffer.begin(cmdBeginInfo);

  // populate the buffer

  vk::ClearValue clearValue;
  // float flash = abs(sin(m_frameNumber / 120.f));
  // clearValue.color.setFloat32({1 - flash, 0.0f, flash, 1.0f});
  clearValue.color.setFloat32({0., 0., 0., 1.});

  vk::ClearValue depthClear;
  depthClear.depthStencil.setDepth(1.f);

  // begin main renderpass
  vk::RenderPassBeginInfo rpInfo;
  rpInfo.renderPass = m_renderPass;
  rpInfo.renderArea.setOffset({0, 0});
  rpInfo.renderArea.extent = m_windowExtent;
  rpInfo.framebuffer = m_framebuffers[swapchainImageIndex];

  auto clearValues = {clearValue, depthClear};
  rpInfo.setClearValues(clearValues);
  get_current_frame().m_mainCommandBuffer.beginRenderPass(
      rpInfo, vk::SubpassContents::eInline);

  draw_objects(get_current_frame().m_mainCommandBuffer, m_renderables.data(),
               m_renderables.size());

  // finish populating the buffer
  get_current_frame().m_mainCommandBuffer.endRenderPass();
  get_current_frame().m_mainCommandBuffer.end();

  vk::SubmitInfo submitInfo;
  submitInfo.setWaitSemaphores(get_current_frame().m_presentSemaphore);
  submitInfo.setSignalSemaphores(get_current_frame().m_renderSemaphore);
  submitInfo.setCommandBuffers(get_current_frame().m_mainCommandBuffer);
  // TODO: wtf?
  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  submitInfo.setWaitDstStageMask(waitStage);
  m_graphicsQueue.submit(submitInfo, get_current_frame().m_renderFence);

  // now present to surface
  vk::PresentInfoKHR presentInfo;
  presentInfo.setSwapchains(m_swapchain);
  presentInfo.setWaitSemaphores(get_current_frame().m_renderSemaphore);
  presentInfo.setImageIndices(swapchainImageIndex);
  (void)m_graphicsQueue.presentKHR(presentInfo);

  if (m_frameNumber % 500 == 0) {
    spdlog::info("Frame {}", m_frameNumber);
  }
  m_frameNumber++;
}

void VulkanEngine::draw_objects(vk::CommandBuffer cmd, RenderObject *first,
                                int count) {
  glm::mat4 projection =
      glm::perspective(glm::radians(70.), 1920. / 1080., 0.1, 200.);
  // TODO: why?
  projection[1][1] *= -1;

  // fill GPU camera data struct
  GPUCameraData camData;
  camData.proj = projection;
  camData.view = m_viewMatrix;
  camData.viewproj = projection * m_viewMatrix;

  // setup sceneData
  int frameIndex = m_frameNumber % FRAME_OVERLAP;
  float framed = m_frameNumber / 288.;
  m_sceneParameters.ambientColor = {sin(framed), 0, cos(framed), 1};

  char *data = (char *)m_allocator.mapMemory(m_cameraSceneBuffer.allocation);
  data += pad_uniform_buffer_size(sizeof(GPUCameraSceneData)) * frameIndex;
  memcpy(data + offsetof(GPUCameraSceneData, cameraData), &camData,
         sizeof(GPUCameraData));
  memcpy(data + offsetof(GPUCameraSceneData, sceneData), &m_sceneParameters,
         sizeof(GPUSceneData));
  m_allocator.unmapMemory(m_cameraSceneBuffer.allocation);

  void *objectData =
      m_allocator.mapMemory((get_current_frame().objectBuffer.allocation));
  GPUObjectData *objectSSBO = (GPUObjectData *)objectData;
  void *objectLightingData = m_allocator.mapMemory(
      (get_current_frame().objectLightingBuffer.allocation));
  GPUObjectLightingData *objectLightingSSBO =
      (GPUObjectLightingData *)objectLightingData;

  // sort renderobjects by material
  std::vector<RenderObject> sortedRenderObjects;
  for (int i = 0; i < count; i++) {
    sortedRenderObjects.push_back(*(first + i));
  }
  std::sort(sortedRenderObjects.begin(), sortedRenderObjects.end(),
            [](RenderObject &a, RenderObject &b) {
              return (uint64_t)a.material < (uint64_t)b.material;
            });

  // render each renderObject
  Mesh *lastMesh = nullptr;
  Material *lastMaterial = nullptr;
  for (int i = 0; i < count; i++) {
    RenderObject &object = sortedRenderObjects[i];
    if (object.material != lastMaterial) {
      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
                       object.material->pipeline);
      lastMaterial = object.material;
      // bind descriptor set when changing pipeline
      uint32_t uniformOffset =
          pad_uniform_buffer_size(sizeof(GPUCameraSceneData)) * frameIndex;
      uint32_t dOffset[] = {uniformOffset, uniformOffset};
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                             object.material->pipelineLayout, 0, 1,
                             &m_globalDescriptorSet, 2, dOffset);
      cmd.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, object.material->pipelineLayout, 1,
          1, &get_current_frame().objectDescriptorSet, 0, nullptr);
      if (object.material->textureSet.has_value()) {
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, object.material->pipelineLayout,
            2, 1, &object.material->textureSet.value(), 0, nullptr);
      }
    }

    MeshPushConstants constants;
    constants.render_matrix = object.transformMatrix;
    // upload mesh via push constants
    cmd.pushConstants(object.material->pipelineLayout,
                      vk::ShaderStageFlagBits::eVertex, 0,
                      sizeof(MeshPushConstants), &constants);

    objectSSBO[i].modelMatrix = object.transformMatrix;
    objectLightingSSBO[i].objectAmbientLighting =
        glm::vec4(i % 3 == 0, i % 3 == 1, i % 3 == 2, 1);

    if (object.mesh != lastMesh) {
      vk::DeviceSize offset = 0;
      cmd.bindVertexBuffers(0, object.mesh->vertexBuffer.buffer, offset);
      lastMesh = object.mesh;
    }

    cmd.draw(object.mesh->vertices.size(), 1, 0, i);
  }

  m_allocator.unmapMemory(get_current_frame().objectBuffer.allocation);
  m_allocator.unmapMemory(get_current_frame().objectLightingBuffer.allocation);
}
} // namespace vkr