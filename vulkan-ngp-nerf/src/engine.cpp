#include "common_includes.h"

#include "SDL_events.h"
#include "SDL_scancode.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <glm/common.hpp>
#include <glm/gtx/string_cast.hpp>

#include "constants.h"
#include "engine.hpp"
#include "mesh.hpp"

namespace vkr {

void VulkanEngine::run() {
  SDL_Event e;
  bool quit = false;

  // main loop
  while (!quit) {
    // Handle events on queue
    SDL_PumpEvents();
    while (SDL_PollEvent(&e) != 0) {
      // close the window when user clicks the X button or alt-f4s
      switch (e.type) {
      case SDL_QUIT:
        quit = true;
        break;
      case SDL_KEYDOWN:
        m_inputs.keyPressed.insert(e.key.keysym.scancode);
        break;
      case SDL_KEYUP:
        m_inputs.keyPressed.erase(e.key.keysym.scancode);
        input_handle_keyup(e.key.keysym.scancode);
        break;
      case SDL_MOUSEMOTION:
        break;
      case SDL_MOUSEWHEEL:
        break;
      }
    }

    for (auto sc : m_inputs.keyPressed) {
      input_handle_keydown(sc);
    }

    spdlog::info("glm viewmat:\n{}", glm::to_string(m_viewMatrix).c_str());
    update_scene();
    draw();
  }
}

void VulkanEngine::cleanup() {
  if (!m_isInitialized) {
    return;
  }

  // wait for gpu idle
  for (FrameData &frame : m_frames) {
    (void)m_device.waitForFences(frame.m_renderFence, true, S_TO_NS);
  }

  m_mainDeletionQueue.flush();

  m_device.destroy();
  m_instance.destroySurfaceKHR(m_surface);
  m_instance.destroy();
  SDL_DestroyWindow(m_window);

  spdlog::info("Engine cleaned up");
}

} // namespace vkr