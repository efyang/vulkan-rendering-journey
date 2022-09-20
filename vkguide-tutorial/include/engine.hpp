#pragma once
#include <deque>
#include <functional>
#include <optional>
#include <set>

#include "SDL_events.h"
#include "SDL_scancode.h"
#include "SDL_shape.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "mesh.hpp"

namespace vkr {

struct Material {
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
};

struct RenderObject {
  Mesh *mesh;
  Material *material;
  glm::mat4 transformMatrix;
};

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()> &&function) {
    deletors.push_back(function);
  }

  void flush() {
    // reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)(); // call the function
    }

    deletors.clear();
  }
};

struct Inputs {
  std::set<SDL_Scancode> keyPressed;
};

class VulkanEngine {
public:
  VulkanEngine();
  void init();
  void run();
  void cleanup();
  void draw();

private:
  std::string m_appName = "Vulkan Engine";
  SDL_Window *m_window;
  bool m_isInitialized = false;
  vk::Extent2D m_windowExtent = vk::Extent2D(1920, 1080);
  uint64_t m_frameNumber = 0;

  // vulkan-specific
  vk::Instance m_instance;
  vk::DebugUtilsMessengerEXT m_debugMessenger;
  vk::PhysicalDevice m_physicalDevice;
  vk::Device m_device;
  vk::SurfaceKHR m_surface;
  void init_vulkan();

  vk::SwapchainKHR m_swapchain;
  vk::Format m_swapchainImageFormat;
  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;
  void init_swapchain();

  vk::Queue m_graphicsQueue;
  uint32_t m_graphicsQueueFamily;

  vk::CommandPool m_commandPool;
  vk::CommandBuffer m_mainCommandBuffer;
  void init_commands();

  vk::RenderPass m_renderPass;
  std::vector<vk::Framebuffer> m_framebuffers;
  void init_default_renderpass();
  void init_framebuffers();

  vk::Semaphore m_presentSemaphore, m_renderSemaphore;
  vk::Fence m_renderFence;
  void init_sync_structures();

  std::optional<vk::ShaderModule> load_shader_module(const char *filePath);
  std::unordered_map<std::string, std::string> m_shaderFiles;
  std::unordered_map<std::string, vk::ShaderModule> m_shaderModules;
  void init_shader_modules();
  void init_pipelines();

  vma::Allocator m_allocator;
  DeletionQueue m_mainDeletionQueue;

  vk::PipelineLayout m_meshPipelineLayout;
  vk::Pipeline m_meshPipeline;

  vk::ImageView m_depthImageView;
  AllocatedImage m_depthImage;
  vk::Format m_depthFormat;

  std::vector<RenderObject> m_renderables;
  std::unordered_map<std::string, Material> m_materials;
  std::unordered_map<std::string, Mesh> m_meshes;
  Material *create_material(VkPipeline pipeline, VkPipelineLayout layout,
                            const std::string &name);
  Material *get_material(const std::string &name);
  Mesh *get_mesh(const std::string &name);
  void draw_objects(vk::CommandBuffer cmd, RenderObject *first, int count);

  Mesh m_triangleMesh;
  Mesh m_monkeyMesh;
  void load_meshes();
  void upload_mesh(Mesh &mesh);

  void init_scene();

  glm::mat4 m_viewMatrix;

  Inputs m_inputs;
  void input_handle_keydown(SDL_Scancode &key);
  void input_handle_keyup(SDL_Scancode &key);
};

} // namespace vkr