#pragma once
#include <optional>

#include "SDL_shape.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.hpp>

namespace vkr {

class VkEngine {
public:
  VkEngine();
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
  void init_pipelines();
  vk::PipelineLayout m_trianglePipelineLayout;
  vk::Pipeline m_trianglePipeline;
};

} // namespace vkr