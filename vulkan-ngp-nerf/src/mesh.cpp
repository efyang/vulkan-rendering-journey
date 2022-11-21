#include "mesh.hpp"
#include <cstddef>
#include <filesystem>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

VertexInputDescription Vertex::get_vertex_description() {
  VertexInputDescription description;

  // 1 vertex buffer binding, per vertex rate
  vk::VertexInputBindingDescription mainBinding;
  mainBinding.setBinding(0)
      .setInputRate(vk::VertexInputRate::eVertex)
      .setStride(sizeof(Vertex));

  description.bindings.push_back(mainBinding);

  // position at loc 0
  vk::VertexInputAttributeDescription positionAttribute(
      0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
  vk::VertexInputAttributeDescription normalAttribute(
      1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal));
  vk::VertexInputAttributeDescription colorAttribute(
      2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color));
  vk::VertexInputAttributeDescription uvAttribute(
      3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));

  description.attributes.push_back(positionAttribute);
  description.attributes.push_back(normalAttribute);
  description.attributes.push_back(colorAttribute);
  description.attributes.push_back(uvAttribute);

  return description;
}

std::optional<Mesh> Mesh::load_from_obj(const char *fileName) {
  // attrib will contain the vertex arrays of the file
  tinyobj::attrib_t attrib;
  // shapes contains the info for each separate object in the file
  std::vector<tinyobj::shape_t> shapes;
  // materials contains the information about the material of each shape, but we
  // won't use it.
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  std::filesystem::path filePath = fileName;
  std::filesystem::path fileDirectory = filePath.parent_path();

  tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fileName,
                   fileDirectory.c_str());
  if (!warn.empty()) {
    spdlog::warn(warn);
  }

  if (!err.empty()) {
    spdlog::error(err);
    return {};
  }

  Mesh m;
  glm::vec3 centroid = glm::vec3(0.);
  std::unordered_map<Vertex, uint32_t> dedupVertices;
  for (const auto &s : shapes) {
    for (const auto &idx : s.mesh.indices) {
      Vertex new_vert;
      // position
      for (int i = 0; i < 3; i++) {
        new_vert.position[i] = attrib.vertices[3 * idx.vertex_index + i];
        new_vert.normal[i] = attrib.normals[3 * idx.normal_index + i];
        new_vert.color[i] = attrib.colors[3 * idx.vertex_index + i];
      }

      if (idx.texcoord_index != -1) {
        tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
        tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

        new_vert.uv.x = ux;
        // vulkan specific
        new_vert.uv.y = 1 - uy;
      }
      centroid += new_vert.position;

      auto existingVert = dedupVertices.find(new_vert);
      if (existingVert != dedupVertices.end()) {
        // exists, use the known index
        m.indices.push_back(existingVert->second);
      } else {
        // doesn't exist, put it in dedup and push back
        m.vertices.push_back(new_vert);
        dedupVertices[new_vert] = m.vertices.size() - 1;
        m.indices.push_back(dedupVertices[new_vert]);
      }
    }
  }
  // center the mesh
  centroid /= (double)m.indices.size();
  for (auto &v : m.vertices) {
    v.position -= centroid;
  }

  return m;
}