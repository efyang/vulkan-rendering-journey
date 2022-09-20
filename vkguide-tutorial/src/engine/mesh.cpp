#include "common_includes.h"

#include "engine.hpp"
#include "mesh.hpp"

namespace vkr {
void VulkanEngine::load_meshes() {
  m_triangleMesh.vertices.resize(3);
  m_triangleMesh.vertices[0].position = {1, 1, 0};
  m_triangleMesh.vertices[1].position = {-1, 1, 0};
  m_triangleMesh.vertices[2].position = {0, -1, 0};

  m_triangleMesh.vertices[0].color = {1, 0, 0};
  m_triangleMesh.vertices[1].color = {0, 1, 0};
  m_triangleMesh.vertices[2].color = {0, 0, 1};

  auto tryMonkeyMesh =
      Mesh::load_from_obj("thirdparty/vulkan-guide/assets/monkey_smooth.obj");
  if (!tryMonkeyMesh.has_value()) {
    throw std::runtime_error("Cannot continue without monkey mesh");
  } else {
    m_monkeyMesh = tryMonkeyMesh.value();
    spdlog::info("Loaded monkey mesh from obj");
  }

  upload_mesh(m_monkeyMesh);
  spdlog::info("Uploaded monkey mesh");
  m_meshes["monkey"] = m_monkeyMesh;
  upload_mesh(m_triangleMesh);
  spdlog::info("Uploaded triangle mesh");
  m_meshes["triangle"] = m_triangleMesh;
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
  vk::BufferCreateInfo bufferInfo;
  bufferInfo.setSize(mesh.vertices.size() * sizeof(Vertex));
  bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer);

  // let the VMA library know that this data should be writeable by CPU, but
  // also readable by GPU
  vma::AllocationCreateInfo allocInfo;
  allocInfo.setUsage(vma::MemoryUsage::eCpuToGpu);

  auto allocated = m_allocator.createBuffer(bufferInfo, allocInfo);
  mesh.vertexBuffer.buffer = allocated.first;
  mesh.vertexBuffer.allocation = allocated.second;
  m_mainDeletionQueue.push_function([=]() {
    m_allocator.destroyBuffer(mesh.vertexBuffer.buffer,
                              mesh.vertexBuffer.allocation);
  });

  void *data = m_allocator.mapMemory(mesh.vertexBuffer.allocation);
  std::memcpy(data, mesh.vertices.data(), bufferInfo.size);
  m_allocator.unmapMemory(mesh.vertexBuffer.allocation);

  spdlog::info("Uploaded mesh of size {}", bufferInfo.size);
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