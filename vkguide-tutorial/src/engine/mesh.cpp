#include "mesh.hpp"
#include "common_includes.h"
#include "constants.h"
#include "engine.hpp"

#include <iostream>
#include <string>

namespace vkr {

void VulkanEngine::load_obj_mesh(const std::string &path,
                                 const std::string &name) {
  auto tryMesh = Mesh::load_from_obj(path.c_str());
  if (!tryMesh.has_value()) {
    throw std::runtime_error(
        fmt::format("Failed to load mesh {} with path {}", name, path));
  } else {
    spdlog::info(
        fmt::format("Successfully loaded mesh {} with path {}", name, path));
    m_meshes[name] = tryMesh.value();
    upload_mesh(m_meshes[name]);
  }
}

void VulkanEngine::load_meshes() {
  m_triangleMesh.vertices.resize(3);
  m_triangleMesh.vertices[0].position = {1, 1, 0};
  m_triangleMesh.vertices[1].position = {-1, 1, 0};
  m_triangleMesh.vertices[2].position = {0, -1, 0};

  m_triangleMesh.vertices[0].color = {1, 0, 0};
  m_triangleMesh.vertices[1].color = {0, 1, 0};
  m_triangleMesh.vertices[2].color = {0, 0, 1};

  load_obj_mesh("thirdparty/vulkan-guide/assets/monkey_smooth.obj", "monkey");
  load_obj_mesh("thirdparty/OpenGL/Binaries/bunny.obj", "bunny");
  load_obj_mesh("thirdparty/vulkan-guide/assets/lost_empire.obj", "empire");

  upload_mesh(m_triangleMesh);
  spdlog::info("Uploaded triangle mesh");
  m_meshes["triangle"] = m_triangleMesh;
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
  size_t vertexBufferSize = mesh.vertices.size() * sizeof(Vertex);
  AllocatedBuffer stagingBuffer =
      create_buffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                    vma::MemoryUsage::eCpuOnly);
  void *data = m_allocator.mapMemory(stagingBuffer.allocation);
  std::memcpy(data, mesh.vertices.data(), vertexBufferSize);
  m_allocator.unmapMemory(stagingBuffer.allocation);

  mesh.vertexBuffer = create_buffer(vertexBufferSize,
                                    vk::BufferUsageFlagBits::eVertexBuffer |
                                        vk::BufferUsageFlagBits::eTransferDst,
                                    vma::MemoryUsage::eGpuOnly);

  immediate_submit([&](vk::CommandBuffer cmd) {
    vk::BufferCopy copy(0, 0, vertexBufferSize);
    cmd.copyBuffer(stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
  });

  m_mainDeletionQueue.push_function([&]() {
    m_allocator.destroyBuffer(mesh.vertexBuffer.buffer,
                              mesh.vertexBuffer.allocation);
  });
  m_allocator.destroyBuffer(stagingBuffer.buffer, stagingBuffer.allocation);

  spdlog::info("Uploaded mesh of size {}", vertexBufferSize);
}

void VulkanEngine::immediate_submit(
    std::function<void(vk::CommandBuffer cmd)> &&function) {
  vk::CommandBuffer cmd = m_uploadContext.commandBuffer;

  vk::CommandBufferBeginInfo cmdBeginInfo(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  cmd.begin(cmdBeginInfo);
  function(cmd);
  cmd.end();

  vk::SubmitInfo submit;
  submit.setCommandBuffers(cmd);
  m_graphicsQueue.submit(submit, m_uploadContext.uploadFence);

  (void)m_device.waitForFences(m_uploadContext.uploadFence, true, S_TO_NS);
  m_device.resetFences(m_uploadContext.uploadFence);

  m_device.resetCommandPool(m_uploadContext.commandPool);
}

// Materials and meshes

Mesh *VulkanEngine::get_mesh(const std::string &name) {
  auto it = m_meshes.find(name);
  if (it == m_meshes.end()) {
    return nullptr;
  } else {
    return &(*it).second;
  }
}

} // namespace vkr