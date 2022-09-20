#include "mesh.hpp"
#include <cstddef>
#include <filesystem>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>
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

  description.attributes.push_back(positionAttribute);
  description.attributes.push_back(normalAttribute);
  description.attributes.push_back(colorAttribute);

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
  for (tinyobj::shape_t s : shapes) {
    // loop over faces
    size_t index_offset = 0;
    for (auto f : s.mesh.num_face_vertices) {
      // hardcode to tris
      size_t fv = 3;
      for (size_t v = 0; v < fv; v++) {
        tinyobj::index_t idx = s.mesh.indices[index_offset + v];

        Vertex new_vert;
        // position
        for (int i = 0; i < 3; i++) {
          new_vert.position[i] = attrib.vertices[fv * idx.vertex_index + i];
          new_vert.normal[i] = attrib.normals[fv * idx.normal_index + i];
          // set vert colors as normals for now
          new_vert.color[i] = attrib.colors[fv * idx.vertex_index + i];
          // new_vert.color = (new_vert.normal + 1.f) / 2.f;
        }
        m.vertices.push_back(new_vert);
      }
      index_offset += fv;
    }
  }
  return m;
}