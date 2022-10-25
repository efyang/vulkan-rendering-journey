#include "common_includes.h"

#include "engine.hpp"
#include "mesh.hpp"

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

  upload_mesh(m_triangleMesh);
  spdlog::info("Uploaded triangle mesh");
  m_meshes["triangle"] = m_triangleMesh;
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
  // let the VMA library know that this data should be writeable by CPU, but
  // also readable by GPU
  size_t vertexBufferSize = mesh.vertices.size() * sizeof(Vertex);
  mesh.vertexBuffer =
      create_buffer(vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                    vma::MemoryUsage::eCpuToGpu);

  m_mainDeletionQueue.push_function([=]() {
    m_allocator.destroyBuffer(mesh.vertexBuffer.buffer,
                              mesh.vertexBuffer.allocation);
  });

  void *data = m_allocator.mapMemory(mesh.vertexBuffer.allocation);
  std::memcpy(data, mesh.vertices.data(), vertexBufferSize);
  m_allocator.unmapMemory(mesh.vertexBuffer.allocation);

  spdlog::info("Uploaded mesh of size {}", vertexBufferSize);
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