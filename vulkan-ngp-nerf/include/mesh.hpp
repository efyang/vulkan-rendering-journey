#pragma once

#include "types.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <vector>

struct VertexInputDescription {
  std::vector<vk::VertexInputBindingDescription> bindings;
  std::vector<vk::VertexInputAttributeDescription> attributes;

  vk::PipelineVertexInputStateCreateFlags flags = {};
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;

  static VertexInputDescription get_vertex_description();
  bool operator==(const Vertex &rhs) const {
    return (position == rhs.position) && (normal == rhs.normal) &&
           (color == rhs.color) && (uv == rhs.uv);
  }
};

namespace std {
template <> struct hash<Vertex> {
  std::size_t operator()(const Vertex &k) const {
    auto vec3Hasher = hash<glm::vec3>();
    return (vec3Hasher(k.position) ^ (vec3Hasher(k.normal) << 1) ^
            (vec3Hasher(k.color) << 2) ^ (hash<glm::vec2>()(k.uv) << 3));
  }
};
}; // namespace std

struct Mesh {
  // TODO: merge vbuf and indexbuf into 1
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // both vertices and indices
  // [vertices | indices]
  AllocatedBuffer combinedVertexBuffer;

  static std::optional<Mesh> load_from_obj(const char *fileName);
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};

struct Material {
  std::optional<vk::DescriptorSet> textureSet; // default to no texture
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
};

struct RenderObject {
  Mesh *mesh;
  Material *material;
  glm::mat4 transformMatrix;

  // ~RenderObject() { spdlog::warn("Renderobject destroyed"); }
};