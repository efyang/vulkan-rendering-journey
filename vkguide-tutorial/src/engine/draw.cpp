#include "common_includes.h"

// glm
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "constants.h"
#include "engine.hpp"

namespace vkr {

void VulkanEngine::draw() {
  // timeout in ns
  (void)m_device.waitForFences(m_renderFence, true, S_TO_NS);
  m_device.resetFences(m_renderFence);

  // request swapchain image
  uint32_t swapchainImageIndex =
      m_device
          .acquireNextImageKHR(m_swapchain, S_TO_NS, m_presentSemaphore,
                               nullptr)
          .value;

  m_mainCommandBuffer.reset();
  vk::CommandBufferBeginInfo cmdBeginInfo;
  cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  m_mainCommandBuffer.begin(cmdBeginInfo);

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
  m_mainCommandBuffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

  draw_objects(m_mainCommandBuffer, m_renderables.data(), m_renderables.size());

  // finish populating the buffer
  m_mainCommandBuffer.endRenderPass();
  m_mainCommandBuffer.end();

  vk::SubmitInfo submitInfo;
  submitInfo.setWaitSemaphores(m_presentSemaphore);
  submitInfo.setSignalSemaphores(m_renderSemaphore);
  submitInfo.setCommandBuffers(m_mainCommandBuffer);
  // TODO: wtf?
  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  submitInfo.setWaitDstStageMask(waitStage);
  m_graphicsQueue.submit(submitInfo, m_renderFence);

  // now present to surface
  vk::PresentInfoKHR presentInfo;
  presentInfo.setSwapchains(m_swapchain);
  presentInfo.setWaitSemaphores(m_renderSemaphore);
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
      glm::perspective(glm::radians(70.), 1700. / 900., 0.1, 200.);
  // TODO: why?
  projection[1][1] *= -1;

  Mesh *lastMesh = nullptr;
  Material *lastMaterial = nullptr;
  for (int i = 0; i < count; i++) {
    RenderObject &object = first[i];
    if (object.material != lastMaterial) {
      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
                       object.material->pipeline);
      lastMaterial = object.material;
    }

    glm::mat4 model = object.transformMatrix;
    glm::mat4 mesh_matrix = projection * m_viewMatrix * model;

    MeshPushConstants constants;
    constants.render_matrix = mesh_matrix;
    // upload mesh via push constants
    cmd.pushConstants(object.material->pipelineLayout,
                      vk::ShaderStageFlagBits::eVertex, 0,
                      sizeof(MeshPushConstants), &constants);

    if (object.mesh != lastMesh) {
      vk::DeviceSize offset = 0;
      cmd.bindVertexBuffers(0, object.mesh->vertexBuffer.buffer, offset);
      lastMesh = object.mesh;
    }

    cmd.draw(object.mesh->vertices.size(), 1, 0, 0);
  }
}
} // namespace vkr