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
#include "types.hpp"

namespace vkr {

// TODO: turn this into a GPU data section?
struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
};

struct GPUSceneData {
  // stick to vec4 and mat4, avoid mixing dtypes, still need to pad
  glm::vec4 fogColor;     // w is for exponent
  glm::vec4 fogDistances; // x min y max, zw unused
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection; // w for sun power
  glm::vec4 sunlightColor;
};

struct GPUObjectData {
  glm::mat4 modelMatrix;
};

struct FrameData {
  vk::Semaphore m_presentSemaphore, m_renderSemaphore;
  vk::Fence m_renderFence;

  vk::CommandPool m_commandPool;
  vk::CommandBuffer m_mainCommandBuffer;

  AllocatedBuffer cameraBuffer;
  vk::DescriptorSet globalDescriptor;

  // ssbo of object data
  AllocatedBuffer objectBuffer;
  vk::DescriptorSet objectDescriptor;
};

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

constexpr unsigned int FRAME_OVERLAP = 3;

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
  vk::PhysicalDeviceProperties m_gpuProperties;
  void init_vulkan();

  vk::SwapchainKHR m_swapchain;
  vk::Format m_swapchainImageFormat;
  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;
  void init_swapchain();

  vk::Queue m_graphicsQueue;
  uint32_t m_graphicsQueueFamily;

  FrameData m_frames[FRAME_OVERLAP];
  // frame that is currently being rendered
  const FrameData &get_current_frame();

  void init_commands();
  void init_sync_structures();

  AllocatedBuffer create_buffer(size_t allocSize, vk::BufferUsageFlags usage,
                                vma::MemoryUsage memoryUsage);

  vk::DescriptorSetLayout m_globalSetLayout;
  vk::DescriptorSetLayout m_objectSetLayout;
  vk::DescriptorPool m_descriptorPool;
  size_t pad_uniform_buffer_size(size_t originalSize);
  void init_descriptors();

  vk::RenderPass m_renderPass;
  std::vector<vk::Framebuffer> m_framebuffers;
  void init_default_renderpass();
  void init_framebuffers();

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
  void load_meshes();
  void load_obj_mesh(const std::string &path, const std::string &name);
  void upload_mesh(Mesh &mesh);

  void init_scene();

  glm::mat4 m_viewMatrix;
  GPUSceneData m_sceneParameters;
  AllocatedBuffer m_sceneParameterBuffer;

  Inputs m_inputs;
  void input_handle_keydown(SDL_Scancode &key);
  void input_handle_keyup(SDL_Scancode &key);
};

} // namespace vkr