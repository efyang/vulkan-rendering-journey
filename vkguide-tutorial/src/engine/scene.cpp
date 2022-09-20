#include "common_includes.h"

// glm
#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "engine.hpp"

namespace vkr {

void VulkanEngine::init_scene() {
  glm::vec3 cameraPosition = {0., -6., -10.};
  m_viewMatrix = glm::translate(glm::mat4(1.), cameraPosition);

  RenderObject monkey;
  monkey.mesh = get_mesh("monkey");
  monkey.material = get_material("defaultmesh");
  monkey.transformMatrix = glm::mat4(1.0f);

  m_renderables.push_back(monkey);

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

} // namespace vkr