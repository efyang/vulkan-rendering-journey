#pragma once

#include "types.hpp"
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
};

struct Mesh {
  std::vector<Vertex> vertices;
  AllocatedBuffer vertexBuffer;

  static std::optional<Mesh> load_from_obj(const char *fileName);
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};