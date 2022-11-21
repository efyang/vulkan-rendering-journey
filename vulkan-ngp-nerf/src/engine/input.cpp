#include "common_includes.h"

#include <glm/ext/matrix_transform.hpp>

#include "engine.hpp"

namespace vkr {
void VulkanEngine::input_handle_keydown(SDL_Scancode &key) {
  spdlog::info("Keydown on key {}", key);

  float translateCameraSpeed = 0.1;
  glm::vec3 translateCameraDirection = glm::vec3(0.);

  switch (key) {
  case SDL_SCANCODE_W:
    translateCameraDirection = glm::vec3(0., 0., 1.);
    break;
  case SDL_SCANCODE_S:
    translateCameraDirection = glm::vec3(0., 0., -1.);
    break;
  case SDL_SCANCODE_A:
    translateCameraDirection = glm::vec3(1., 0., 0.);
    break;
  case SDL_SCANCODE_D:
    translateCameraDirection = glm::vec3(-1., 0., 0.);
    break;
  case SDL_SCANCODE_E:
    translateCameraDirection = glm::vec3(0., -1., 0.);
    break;
  case SDL_SCANCODE_Q:
    translateCameraDirection = glm::vec3(0., 1., 0.);
    break;
  default:
    break;
  }

  m_viewMatrix = glm::translate(m_viewMatrix, translateCameraDirection *
                                                  translateCameraSpeed);
}

void VulkanEngine::input_handle_keyup(SDL_Scancode &key) {
  spdlog::info("Keyup on key {}", key);
}

} // namespace vkr