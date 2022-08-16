#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "SDL_events.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <fstream>

#include "VkBootstrap.h"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

#include "constants.h"
#include "engine.hpp"
#include "src/pipeline.hpp"
#include "types.hpp"

namespace vkr {

VkEngine::VkEngine() {}

void VkEngine::init() {
  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  // create blank SDL window for our application
  m_window = SDL_CreateWindow(
      m_appName.c_str(),       // window title
      SDL_WINDOWPOS_UNDEFINED, // window position x (don't care)
      SDL_WINDOWPOS_UNDEFINED, // window position y (don't care)
      m_windowExtent.width,    // window width in pixels
      m_windowExtent.height,   // window height in pixels
      window_flags);

  init_vulkan();
  init_swapchain();
  init_commands();
  init_default_renderpass();
  init_framebuffers();
  init_sync_structures();
  init_pipelines();

  // everything went fine
  m_isInitialized = true;
}

void VkEngine::init_vulkan() {
  vkb::InstanceBuilder instanceBuilder;
  auto vkb_inst = instanceBuilder.set_app_name(m_appName.c_str())
                      .request_validation_layers(true)
                      .require_api_version(1, 3, 0)
                      .use_default_debug_messenger()
                      .build()
                      .value();

  m_instance = vkb_inst.instance;
  m_debugMessenger = vkb_inst.debug_messenger;
  spdlog::info("Initialized vulkan instance via vkb");

  SDL_Vulkan_CreateSurface(m_window, m_instance, (VkSurfaceKHR *)&m_surface);
  spdlog::info("Initialized vulkan surface via sdl");

  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vk::PhysicalDeviceVulkan13Features features_13;
  features_13.dynamicRendering = true;
  selector = selector.set_minimum_version(1, 3)
                 .set_surface(m_surface)
                 .set_required_features_13(
                     static_cast<VkPhysicalDeviceVulkan13Features>(features_13))
      /*
      .add_required_extension("VK_KHR_acceleration_structure")
      .add_required_extension("VK_KHR_ray_tracing_pipeline")
      .add_required_extension("VK_NV_mesh_shader")
      */
      ;
  auto availableDevices = selector.select_device_names();
  uint32_t idx = 0;
  for (std::string name : availableDevices.value()) {
    spdlog::info("Available physical device {}: {}", idx++, name);
  }

  vkb::PhysicalDevice physicalDevice = selector.select().value();
  vkb::DeviceBuilder deviceBuilder(physicalDevice);
  vkb::Device vkbDevice = deviceBuilder.build().value();

  m_device = vkbDevice.device;
  m_physicalDevice = vkbDevice.physical_device;

  spdlog::info("Initialized physical device: {}",
               m_physicalDevice.getProperties().deviceName);

  m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  m_graphicsQueueFamily =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  spdlog::info("Grabbed graphics queue with family {}", m_graphicsQueueFamily);
}

void VkEngine::init_swapchain() {
  vkb::SwapchainBuilder swapchainBuilder(m_physicalDevice, m_device, m_surface);
  vkb::Swapchain vkbSwapchain =
      swapchainBuilder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(m_windowExtent.width, m_windowExtent.height)
          .build()
          .value();
  m_swapchain = vkbSwapchain.swapchain;
  m_swapchainImageFormat = vk::Format(vkbSwapchain.image_format);

  m_swapchainImages = vkbSwapchain.get_images().value();
  m_swapchainImageViews = vkbSwapchain.get_image_views().value();
  spdlog::info("Successfully initialized swapchain with {} images",
               vkbSwapchain.image_count);
}

void VkEngine::init_commands() {
  vk::CommandPoolCreateFlags commandPoolCreateFlags;
  // allow resetting individual command buffers
  commandPoolCreateFlags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  vk::CommandPoolCreateInfo commandPoolCreateInfo(commandPoolCreateFlags,
                                                  m_graphicsQueueFamily);
  m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
      m_commandPool, VULKAN_HPP_NAMESPACE::CommandBufferLevel::ePrimary, 1);
  m_mainCommandBuffer =
      m_device.allocateCommandBuffers(commandBufferAllocateInfo)[0];

  spdlog::info("Allocated {} command buffers",
               commandBufferAllocateInfo.commandBufferCount);
}

void VkEngine::init_default_renderpass() {
  vk::AttachmentDescription colorAttachment;
  colorAttachment.format = m_swapchainImageFormat;
  // no msaa
  colorAttachment.samples = vk::SampleCountFlagBits::e1;
  // clear on load
  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  // store on end
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  // layout for presentation
  colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  vk::AttachmentReference colorAttachmentRef(
      {}, vk::ImageLayout::eColorAttachmentOptimal);

  // 1 subpass
  vk::SubpassDescription subpass;
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  vk::RenderPassCreateInfo renderPassInfo;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  m_renderPass = m_device.createRenderPass(renderPassInfo);

  spdlog::info("Initialized renderpass");
}

void VkEngine::init_framebuffers() {
  vk::FramebufferCreateInfo fbInfo;
  fbInfo.renderPass = m_renderPass;
  fbInfo.attachmentCount = 1;
  fbInfo.width = m_windowExtent.width;
  fbInfo.height = m_windowExtent.height;
  fbInfo.layers = 1;

  m_framebuffers = std::vector<vk::Framebuffer>(m_swapchainImages.size());
  for (uint32_t i = 0; i < m_swapchainImages.size(); i++) {
    auto iv = vk::ImageView(m_swapchainImageViews[i]);
    fbInfo.pAttachments = &iv;
    m_framebuffers[i] = m_device.createFramebuffer(fbInfo);
  }

  spdlog::info("Initialized framebuffers");
}

void VkEngine::init_sync_structures() {
  vk::FenceCreateInfo fenceCreateInfo;
  // create so that the fence is default signaled
  fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
  m_renderFence = m_device.createFence(fenceCreateInfo);

  vk::SemaphoreCreateInfo semaphoreCreateInfo;
  m_presentSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
  m_renderSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
}

void VkEngine::run() {
  SDL_Event e;
  bool quit = false;

  // main loop
  while (!quit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      // close the window when user clicks the X button or alt-f4s
      switch (e.type) {
      case SDL_QUIT:
        quit = true;
        break;
      case SDL_KEYDOWN:
        std::printf("Keydown %d\n", e.key.keysym.scancode);
        break;
      case SDL_KEYUP:
        std::printf("Keyup %d\n", e.key.keysym.scancode);
        break;
      }
    }

    draw();
  }
}

void VkEngine::cleanup() {
  if (!m_isInitialized) {
    return;
  }

  m_device.destroySwapchainKHR(m_swapchain);

  m_device.destroyRenderPass(m_renderPass);
  for (uint32_t i = 0; i < m_framebuffers.size(); i++) {
    m_device.destroyFramebuffer(m_framebuffers[i]);
    m_device.destroyImageView(m_swapchainImageViews[i]);
  }

  m_device.destroyCommandPool(m_commandPool);

  m_device.destroySemaphore(m_renderSemaphore);
  m_device.destroySemaphore(m_presentSemaphore);
  m_device.destroyFence(m_renderFence);

  m_device.destroy();
  m_instance.destroySurfaceKHR(m_surface);
  vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
  m_instance.destroy();
  SDL_DestroyWindow(m_window);
  spdlog::info("Engine cleaned up");
}

std::optional<vk::ShaderModule>
VkEngine::load_shader_module(const char *filePath) {
  // ate needed for tellg
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    spdlog::error("Shader file {} is already open!", filePath);
    return {};
  }
  size_t fsize = (size_t)file.tellg();
  std::vector<uint32_t> buffer((fsize + sizeof(uint32_t) - 1) /
                               sizeof(uint32_t));
  file.seekg(0);
  file.read((char *)buffer.data(), fsize);
  file.close();

  vk::ShaderModuleCreateInfo createInfo;
  createInfo.setCode(buffer);

  return m_device.createShaderModule(createInfo);
}

void VkEngine::init_pipelines() {
  auto triFrag = load_shader_module("build/assets/shaders/triangle.frag.spv");
  if (!triFrag.has_value()) {
    spdlog::error("Failed to load triangle fragment shader");
    throw std::runtime_error("Cannot continue");
  } else {
    spdlog::info("Loaded triangle fragment shader successfully");
  }

  auto triVert = load_shader_module("build/assets/shaders/triangle.vert.spv");
  if (!triVert.has_value()) {
    spdlog::error("Failed to load triangle vertex shader");
    throw std::runtime_error("Cannot continue");
  } else {
    spdlog::info("Loaded triangle vertex shader successfully");
  }

  vk::PipelineLayoutCreateInfo pipeline_layout_info =
      PipelineBuilder::default_pipeline_layout_create_info();
  m_trianglePipelineLayout =
      m_device.createPipelineLayout(pipeline_layout_info);
  spdlog::info("Created pipeline layout");

  PipelineBuilder pipelineBuilder;
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex, triVert.value()));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment, triFrag.value()));

  // vertex input controls how to read vertices from vertex buffers. We aren't
  // using it yet
  pipelineBuilder.vertexInputInfo =
      PipelineBuilder::default_vertex_input_state_create_info();

  pipelineBuilder.inputAssembly =
      PipelineBuilder::default_pipeline_input_assembly_create_info(
          vk::PrimitiveTopology::eTriangleList);

  pipelineBuilder.viewport.x = 0;
  pipelineBuilder.viewport.y = 0;
  pipelineBuilder.viewport.width = m_windowExtent.width;
  pipelineBuilder.viewport.height = m_windowExtent.height;
  pipelineBuilder.viewport.minDepth = 0;
  pipelineBuilder.viewport.maxDepth = 1;

  pipelineBuilder.scissor.setOffset({0, 0});
  pipelineBuilder.scissor.setExtent(m_windowExtent);

  pipelineBuilder.rasterizer =
      PipelineBuilder::default_rasterization_state_create_info(
          vk::PolygonMode::eFill);

  pipelineBuilder.multisampling =
      PipelineBuilder::default_multisampling_state_create_info();
  pipelineBuilder.colorBlendAttachment =
      PipelineBuilder::default_color_blend_attachment_state();
  pipelineBuilder.pipelineLayout = m_trianglePipelineLayout;
  m_trianglePipeline = pipelineBuilder.build(m_device, m_renderPass);
  spdlog::info("Finished building pipeline");
}

void VkEngine::draw() {
  // timeout in ns
  (void)m_device.waitForFences(m_renderFence, true, S_TO_NS);
  m_device.resetFences(m_renderFence);

  // request swapchain image
  uint32_t swapchainImageIndex =
      m_device
          .acquireNextImageKHR(m_swapchain, S_TO_NS, m_presentSemaphore,
                               nullptr)
          .value;

  m_mainCommandBuffer.reset();
  vk::CommandBufferBeginInfo cmdBeginInfo;
  cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  m_mainCommandBuffer.begin(cmdBeginInfo);

  // populate the buffer

  vk::ClearValue clearValue;
  float flash = abs(sin(m_frameNumber / 120.f));
  clearValue.color.setFloat32({1 - flash, 0.0f, flash, 1.0f});
  // begin main renderpass
  vk::RenderPassBeginInfo rpInfo;
  rpInfo.renderPass = m_renderPass;
  rpInfo.renderArea.setOffset({0, 0});
  rpInfo.renderArea.extent = m_windowExtent;
  rpInfo.framebuffer = m_framebuffers[swapchainImageIndex];

  rpInfo.setClearValues(clearValue);
  m_mainCommandBuffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

  m_mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   m_trianglePipeline);
  m_mainCommandBuffer.draw(3, 1, 0, 0);

  // finish populating the buffer
  m_mainCommandBuffer.endRenderPass();
  m_mainCommandBuffer.end();

  vk::SubmitInfo submitInfo;
  submitInfo.setWaitSemaphores(m_presentSemaphore);
  submitInfo.setSignalSemaphores(m_renderSemaphore);
  submitInfo.setCommandBuffers(m_mainCommandBuffer);
  // TODO: wtf?
  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  submitInfo.setWaitDstStageMask(waitStage);
  m_graphicsQueue.submit(submitInfo, m_renderFence);

  // now present to surface
  vk::PresentInfoKHR presentInfo;
  presentInfo.setSwapchains(m_swapchain);
  presentInfo.setWaitSemaphores(m_renderSemaphore);
  presentInfo.setImageIndices(swapchainImageIndex);
  (void)m_graphicsQueue.presentKHR(presentInfo);

  if (m_frameNumber % 500 == 0) {
    spdlog::info("Frame {}", m_frameNumber);
  }
  m_frameNumber++;
}

} // namespace vkr