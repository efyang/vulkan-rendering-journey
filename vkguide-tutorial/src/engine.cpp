#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "SDL_events.h"
#include "SDL_scancode.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <fstream>

#include "VkBootstrap.h"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

#include <glm/gtx/transform.hpp>
#include <vulkan/vulkan_enums.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.hpp>

#include "constants.h"
#include "engine.hpp"
#include "pipeline.hpp"
#include "types.hpp"

namespace vkr {

VulkanEngine::VulkanEngine() {}

void VulkanEngine::init() {
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
  load_meshes();
  init_shader_modules();
  init_pipelines();

  // everything went fine
  m_isInitialized = true;
}

void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder instanceBuilder;
  auto vkb_inst = instanceBuilder.set_app_name(m_appName.c_str())
                      .request_validation_layers(true)
                      .require_api_version(1, 3, 0)
                      .use_default_debug_messenger()
                      .build()
                      .value();

  m_instance = vkb_inst.instance;
  m_debugMessenger = vkb_inst.debug_messenger;

  m_mainDeletionQueue.push_function([=]() {
    vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
  });

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

  vma::AllocatorCreateInfo allocatorInfo;
  allocatorInfo.physicalDevice = m_physicalDevice;
  allocatorInfo.device = m_device;
  allocatorInfo.instance = m_instance;
  m_allocator = vma::createAllocator(allocatorInfo);
  m_mainDeletionQueue.push_function([=]() { m_allocator.destroy(); });
  spdlog::info("Created vma allocator");
}

void VulkanEngine::init_swapchain() {
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

  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroySwapchainKHR(m_swapchain); });
  spdlog::info("Successfully initialized swapchain with {} images",
               vkbSwapchain.image_count);

  // allocate depth image
  vk::Extent3D depthImageExtent(m_windowExtent.width, m_windowExtent.height, 1);
  m_depthFormat = vk::Format::eD32Sfloat;
  vk::ImageCreateInfo dimgInfo;
  dimgInfo.setFormat(m_depthFormat)
      .setImageType(vk::ImageType::e2D)
      .setExtent(depthImageExtent)
      .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
      .setMipLevels(1)
      .setArrayLayers(1)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal);
  vma::AllocationCreateInfo dimgAllocInfo;
  // usage is only a hint, flag actually forces gpu allocation
  dimgAllocInfo.setUsage(vma::MemoryUsage::eGpuOnly)
      .setRequiredFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);
  auto dimg_alloc = m_allocator.createImage(dimgInfo, dimgAllocInfo);
  m_depthImage.image = dimg_alloc.first;
  m_depthImage.allocation = dimg_alloc.second;

  vk::ImageViewCreateInfo dviewInfo;
  vk::ImageSubresourceRange dviewInfoSubresourceRange;
  dviewInfoSubresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth)
      .setBaseMipLevel(0)
      .setLevelCount(1)
      .setBaseArrayLayer(0)
      .setLayerCount(1);
  dviewInfo.setFormat(m_depthFormat)
      .setViewType(vk::ImageViewType::e2D)
      .setImage(m_depthImage.image)
      .setSubresourceRange(dviewInfoSubresourceRange);
  m_depthImageView = m_device.createImageView(dviewInfo);
  m_mainDeletionQueue.push_function([=]() {
    m_device.destroyImageView(m_depthImageView);
    m_allocator.destroyImage(m_depthImage.image, m_depthImage.allocation);
  });

  spdlog::info("Allocated depth image");
}

void VulkanEngine::init_commands() {
  vk::CommandPoolCreateFlags commandPoolCreateFlags;
  // allow resetting individual command buffers
  commandPoolCreateFlags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  vk::CommandPoolCreateInfo commandPoolCreateInfo(commandPoolCreateFlags,
                                                  m_graphicsQueueFamily);
  m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);

  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroyCommandPool(m_commandPool); });

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
      m_commandPool, VULKAN_HPP_NAMESPACE::CommandBufferLevel::ePrimary, 1);
  m_mainCommandBuffer =
      m_device.allocateCommandBuffers(commandBufferAllocateInfo)[0];

  spdlog::info("Allocated {} command buffers",
               commandBufferAllocateInfo.commandBufferCount);
}

void VulkanEngine::init_default_renderpass() {
  vk::AttachmentDescription colorAttachment;
  colorAttachment.format = m_swapchainImageFormat;
  colorAttachment.samples = vk::SampleCountFlagBits::e1;
  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
  vk::AttachmentReference colorAttachmentRef(
      {}, vk::ImageLayout::eColorAttachmentOptimal);

  vk::AttachmentDescription depthAttachment;
  depthAttachment.setFormat(m_depthFormat)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eStencilAttachmentOptimal);
  vk::AttachmentReference depthAttachmentRef;
  depthAttachmentRef.setAttachment(1).setLayout(
      vk::ImageLayout::eDepthStencilAttachmentOptimal);

  // 1 subpass
  vk::SubpassDescription subpass;
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachments(colorAttachmentRef)
      .setPDepthStencilAttachment(&depthAttachmentRef);

  vk::SubpassDependency colorDep;
  colorDep.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setSrcAccessMask({})
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::SubpassDependency depthDep;
  depthDep.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests |
                       vk::PipelineStageFlagBits::eLateFragmentTests)
      .setSrcAccessMask({})
      .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests |
                       vk::PipelineStageFlagBits::eLateFragmentTests)
      .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);

  auto dependencies = {colorDep, depthDep};

  vk::RenderPassCreateInfo renderPassInfo;
  auto attachments = {colorAttachment, depthAttachment};
  renderPassInfo.setAttachments(attachments)
      .setSubpasses(subpass)
      .setDependencies(dependencies);
  m_renderPass = m_device.createRenderPass(renderPassInfo);

  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroyRenderPass(m_renderPass); });
  spdlog::info("Initialized renderpass");
}

void VulkanEngine::init_framebuffers() {
  vk::FramebufferCreateInfo fbInfo;
  fbInfo.renderPass = m_renderPass;
  fbInfo.width = m_windowExtent.width;
  fbInfo.height = m_windowExtent.height;
  fbInfo.layers = 1;

  m_framebuffers = std::vector<vk::Framebuffer>(m_swapchainImages.size());
  for (uint32_t i = 0; i < m_swapchainImages.size(); i++) {
    auto iv = vk::ImageView(m_swapchainImageViews[i]);
    auto attachments = {iv, m_depthImageView};
    fbInfo.setAttachments(attachments);
    m_framebuffers[i] = m_device.createFramebuffer(fbInfo);

    m_mainDeletionQueue.push_function([=]() {
      m_device.destroyFramebuffer(m_framebuffers[i]);
      m_device.destroyImageView(m_swapchainImageViews[i]);
    });
  }

  spdlog::info("Initialized framebuffers");
}

void VulkanEngine::init_sync_structures() {
  vk::FenceCreateInfo fenceCreateInfo;
  // create so that the fence is default signaled
  fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
  m_renderFence = m_device.createFence(fenceCreateInfo);

  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroyFence(m_renderFence); });

  vk::SemaphoreCreateInfo semaphoreCreateInfo;
  m_presentSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
  m_renderSemaphore = m_device.createSemaphore(semaphoreCreateInfo);

  m_mainDeletionQueue.push_function([=]() {
    m_device.destroySemaphore(m_renderSemaphore);
    m_device.destroySemaphore(m_presentSemaphore);
  });
}

void VulkanEngine::run() {
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
        if (e.key.keysym.scancode == SDL_SCANCODE_SPACE) {
          m_selectedShader = (m_selectedShader + 1) % 4;
        }
        break;
      }
    }

    draw();
  }
}

void VulkanEngine::cleanup() {
  if (!m_isInitialized) {
    return;
  }

  // wait for gpu idle
  (void)m_device.waitForFences(m_renderFence, true, S_TO_NS);

  m_mainDeletionQueue.flush();

  m_device.destroy();
  m_instance.destroySurfaceKHR(m_surface);
  m_instance.destroy();
  SDL_DestroyWindow(m_window);
  spdlog::info("Engine cleaned up");
}

std::optional<vk::ShaderModule>
VulkanEngine::load_shader_module(const char *filePath) {
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

  auto sm = m_device.createShaderModule(createInfo);
  // can be deleted right after pipeline is init, but I want to keep them
  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroyShaderModule(sm); });
  return sm;
}

void VulkanEngine::init_shader_modules() {
  m_shaderFiles = {
      {"redtri.vert", "build/assets/shaders/triangle.vert.spv"},
      {"redtri.frag", "build/assets/shaders/triangle.frag.spv"},
      {"fancytri.vert", "build/assets/shaders/fancytriangle.vert.spv"},
      {"fancytri.frag", "build/assets/shaders/fancytriangle.frag.spv"},
      {"trimesh.vert", "build/assets/shaders/tri_mesh.vert.spv"},
  };

  for (auto shader : m_shaderFiles) {
    auto mod = load_shader_module(shader.second.c_str());
    if (!mod.has_value()) {
      spdlog::error("Failed to load {} shader", shader.first);
      throw std::runtime_error("Cannot continue");
    } else {
      spdlog::info("Loaded {} shader successfully", shader.first);
      m_shaderModules[shader.first] = mod.value();
    }
  }
}

void VulkanEngine::init_pipelines() {
  vk::PipelineLayoutCreateInfo pipeline_layout_info =
      PipelineBuilder::default_pipeline_layout_create_info();
  m_trianglePipelineLayout =
      m_device.createPipelineLayout(pipeline_layout_info);
  spdlog::info("Created pipeline layout");

  PipelineBuilder pipelineBuilder;
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex, m_shaderModules["redtri.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment, m_shaderModules["redtri.frag"]));

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
  m_redTrianglePipeline = pipelineBuilder.build(m_device, m_renderPass);

  spdlog::info("Finished building red triangle pipeline");

  pipelineBuilder.shaderStages.clear();

  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex, m_shaderModules["fancytri.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment,
          m_shaderModules["fancytri.frag"]));
  m_fancyTrianglePipeline = pipelineBuilder.build(m_device, m_renderPass);

  spdlog::info("Finished building fancy triangle pipeline");

  vk::PipelineLayoutCreateInfo meshPipelineLayoutInfo =
      PipelineBuilder::default_pipeline_layout_create_info();
  vk::PushConstantRange pushConstant;
  pushConstant.setSize(sizeof(MeshPushConstants))
      .setOffset(0)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex);
  meshPipelineLayoutInfo.setPushConstantRanges(pushConstant);
  m_meshPipelineLayout = m_device.createPipelineLayout(meshPipelineLayoutInfo);

  pipelineBuilder.shaderStages.clear();
  VertexInputDescription vertexDescription = Vertex::get_vertex_description();
  pipelineBuilder.vertexInputInfo.setVertexAttributeDescriptions(
      vertexDescription.attributes);
  pipelineBuilder.vertexInputInfo.setVertexBindingDescriptions(
      vertexDescription.bindings);
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex, m_shaderModules["trimesh.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment,
          m_shaderModules["fancytri.frag"]));
  pipelineBuilder.pipelineLayout = m_meshPipelineLayout;
  pipelineBuilder.depthStencil =
      PipelineBuilder::default_depth_stencil_create_info(
          true, true, vk::CompareOp::eLessOrEqual);
  m_meshPipeline = pipelineBuilder.build(m_device, m_renderPass);

  m_mainDeletionQueue.push_function([=]() {
    m_device.destroyPipeline(m_fancyTrianglePipeline);
    m_device.destroyPipeline(m_redTrianglePipeline);
    m_device.destroyPipeline(m_meshPipeline);
    m_device.destroyPipelineLayout(m_trianglePipelineLayout);
    m_device.destroyPipelineLayout(m_meshPipelineLayout);
  });

  spdlog::info("Finished building mesh triangle pipeline");
}

void VulkanEngine::draw() {
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

  vk::ClearValue depthClear;
  depthClear.depthStencil.setDepth(1.f);

  // begin main renderpass
  vk::RenderPassBeginInfo rpInfo;
  rpInfo.renderPass = m_renderPass;
  rpInfo.renderArea.setOffset({0, 0});
  rpInfo.renderArea.extent = m_windowExtent;
  rpInfo.framebuffer = m_framebuffers[swapchainImageIndex];

  auto clearValues = {clearValue, depthClear};
  rpInfo.setClearValues(clearValues);
  m_mainCommandBuffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

  if (m_selectedShader == 0) {
    m_mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                     m_redTrianglePipeline);
    m_mainCommandBuffer.draw(3, 1, 0, 0);
  } else if (m_selectedShader == 1) {
    m_mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                     m_fancyTrianglePipeline);
    m_mainCommandBuffer.draw(3, 1, 0, 0);
  } else {
    Mesh *mesh;
    if (m_selectedShader == 2) {
      mesh = &m_triangleMesh;
    } else {
      mesh = &m_monkeyMesh;
    }

    m_mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                     m_meshPipeline);
    vk::DeviceSize offset = 0;
    m_mainCommandBuffer.bindVertexBuffers(0, 1, &mesh->vertexBuffer.buffer,
                                          &offset);
    // make a model view matrix for rendering the object
    // camera position
    glm::vec3 camPos = {0.f, 0.f, -2.f};

    glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
    // camera projection
    glm::mat4 projection =
        glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
    projection[1][1] *= -1;
    // model rotation
    glm::mat4 model =
        glm::rotate(glm::mat4{1.0f}, glm::radians(m_frameNumber * 0.4f),
                    glm::vec3(0, 1, 0));

    // calculate final mesh matrix
    glm::mat4 mesh_matrix = projection * view * model;

    MeshPushConstants constants;
    constants.render_matrix = mesh_matrix;
    m_mainCommandBuffer.pushConstants(m_meshPipelineLayout,
                                      vk::ShaderStageFlagBits::eVertex, 0,
                                      sizeof(MeshPushConstants), &constants);

    m_mainCommandBuffer.draw(mesh->vertices.size(), 1, 0, 0);
  }

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

// todo: move to mesh.cpp?
// mesh cache would be cool to avoid reuploading meshes
void VulkanEngine::load_meshes() {
  m_triangleMesh.vertices.resize(3);
  m_triangleMesh.vertices[0].position = {1, 1, 0};
  m_triangleMesh.vertices[1].position = {-1, 1, 0};
  m_triangleMesh.vertices[2].position = {0, -1, 0};

  m_triangleMesh.vertices[0].color = {1, 0, 0};
  m_triangleMesh.vertices[1].color = {0, 1, 0};
  m_triangleMesh.vertices[2].color = {0, 0, 1};

  auto tryMonkeyMesh =
      Mesh::load_from_obj("thirdparty/vulkan-guide/assets/monkey_smooth.obj");
  if (!tryMonkeyMesh.has_value()) {
    throw std::runtime_error("Cannot continue without monkey mesh");
  } else {
    m_monkeyMesh = tryMonkeyMesh.value();
    spdlog::info("Loaded monkey mesh from obj");
  }

  upload_mesh(m_monkeyMesh);
  spdlog::info("Uploaded monkey mesh");
  upload_mesh(m_triangleMesh);
  spdlog::info("Uploaded triangle mesh");
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
  vk::BufferCreateInfo bufferInfo;
  bufferInfo.setSize(mesh.vertices.size() * sizeof(Vertex));
  bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer);

  // let the VMA library know that this data should be writeable by CPU, but
  // also readable by GPU
  vma::AllocationCreateInfo allocInfo;
  allocInfo.setUsage(vma::MemoryUsage::eCpuToGpu);

  auto allocated = m_allocator.createBuffer(bufferInfo, allocInfo);
  mesh.vertexBuffer.buffer = allocated.first;
  mesh.vertexBuffer.allocation = allocated.second;
  m_mainDeletionQueue.push_function([=]() {
    m_allocator.destroyBuffer(mesh.vertexBuffer.buffer,
                              mesh.vertexBuffer.allocation);
  });

  void *data = m_allocator.mapMemory(mesh.vertexBuffer.allocation);
  std::memcpy(data, mesh.vertices.data(), bufferInfo.size);
  m_allocator.unmapMemory(mesh.vertexBuffer.allocation);

  spdlog::info("Uploaded mesh of size {}", bufferInfo.size);
}

} // namespace vkr