// SDL
#include "SDL_events.h"
#include "SDL_scancode.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>

// vkb
#include "VkBootstrap.h"

#include "common_includes.h"

// in package
#include "engine.hpp"
#include "pipeline.hpp"

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
  init_descriptors();
  init_pipelines();
  init_scene();

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
  vk::PhysicalDeviceShaderDrawParametersFeatures
      shader_draw_parameters_features(true);
  vkb::Device vkbDevice =
      deviceBuilder.add_pNext(&shader_draw_parameters_features).build().value();

  m_device = vkbDevice.device;
  m_physicalDevice = vkbDevice.physical_device;
  m_gpuProperties = vkbDevice.physical_device.properties;

  spdlog::info("Initialized physical device: {}",
               m_physicalDevice.getProperties().deviceName);
  spdlog::info("With minimum buffer alignment of {}",
               m_gpuProperties.limits.minUniformBufferOffsetAlignment);

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

  for (FrameData &frame : m_frames) {
    frame.m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);

    m_mainDeletionQueue.push_function(
        [=]() { m_device.destroyCommandPool(frame.m_commandPool); });

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
        frame.m_commandPool, VULKAN_HPP_NAMESPACE::CommandBufferLevel::ePrimary,
        1);
    frame.m_mainCommandBuffer =
        m_device.allocateCommandBuffers(commandBufferAllocateInfo)[0];

    spdlog::info("Allocated {} command buffers",
                 commandBufferAllocateInfo.commandBufferCount);
  }
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
      .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
  vk::AttachmentReference depthAttachmentRef;
  depthAttachmentRef.setAttachment(1);
  depthAttachmentRef.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

  for (FrameData &frame : m_frames) {
    frame.m_renderFence = m_device.createFence(fenceCreateInfo);

    m_mainDeletionQueue.push_function(
        [=]() { m_device.destroyFence(frame.m_renderFence); });

    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    frame.m_presentSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
    frame.m_renderSemaphore = m_device.createSemaphore(semaphoreCreateInfo);

    m_mainDeletionQueue.push_function([=]() {
      m_device.destroySemaphore(frame.m_renderSemaphore);
      m_device.destroySemaphore(frame.m_presentSemaphore);
    });
  }
}

// TODO: move this to utils and make generic (don't depend on engine?)
size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize) {
  // Calculate required alignment based on minimum device offset alignment
  size_t minUboAlignment =
      m_gpuProperties.limits.minUniformBufferOffsetAlignment;
  size_t alignedSize = originalSize;
  if (minUboAlignment > 0) {
    alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
  }
  return alignedSize;
}

void VulkanEngine::init_descriptors() {
  // descriptor pool that holds 10 uniforms
  std::vector<vk::DescriptorPoolSize> sizes = {
      {vk::DescriptorType::eUniformBuffer, 10},
      {vk::DescriptorType::eUniformBufferDynamic, 10},
      {vk::DescriptorType::eStorageBuffer, 10}};

  vk::DescriptorPoolCreateInfo poolInfo;
  poolInfo.setMaxSets(10);
  poolInfo.setPoolSizes(sizes);

  m_descriptorPool = m_device.createDescriptorPool(poolInfo, nullptr);

  // allocate scene parameter buffer
  const size_t sceneParamBufferSize =
      FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));
  m_sceneParameterBuffer = create_buffer(
      sceneParamBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
      vma::MemoryUsage::eCpuToGpu);

  m_mainDeletionQueue.push_function([&]() {
    m_allocator.destroyBuffer(m_sceneParameterBuffer.buffer,
                              m_sceneParameterBuffer.allocation);
  });

  // camera data at 0
  vk::DescriptorSetLayoutBinding camBufferBinding;
  camBufferBinding.setBinding(0);
  camBufferBinding.setDescriptorCount(1);
  camBufferBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
  camBufferBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
  // scene data at 1
  vk::DescriptorSetLayoutBinding sceneBind(
      1, vk::DescriptorType::eUniformBufferDynamic, 1,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

  vk::DescriptorSetLayoutBinding bindings[] = {camBufferBinding, sceneBind};

  vk::DescriptorSetLayoutCreateInfo setInfo;
  setInfo.setBindings(bindings);
  m_globalSetLayout = m_device.createDescriptorSetLayout(setInfo, nullptr);

  vk::DescriptorSetLayoutBinding objectBind(
      0, vk::DescriptorType::eStorageBuffer, 1,
      vk::ShaderStageFlagBits::eVertex);
  vk::DescriptorSetLayoutCreateInfo objectSetInfo;
  objectSetInfo.setBindingCount(1);
  objectSetInfo.setBindings(objectBind);

  m_objectSetLayout = m_device.createDescriptorSetLayout(objectSetInfo);

  m_mainDeletionQueue.push_function([&]() {
    m_device.destroyDescriptorSetLayout(m_globalSetLayout);
    m_device.destroyDescriptorSetLayout(m_objectSetLayout);
    m_device.destroyDescriptorPool(m_descriptorPool);
  });

  // allocate and build the actual camera buffers
  size_t i = 0;
  for (FrameData &frame : m_frames) {
    vk::DescriptorSetAllocateInfo objectSetAlloc;
    objectSetAlloc.setDescriptorPool(m_descriptorPool);
    objectSetAlloc.setSetLayouts(m_objectSetLayout);
    frame.objectDescriptor = m_device.allocateDescriptorSets(objectSetAlloc)[0];

    // allocate camera buffer
    frame.cameraBuffer = create_buffer(sizeof(GPUCameraData),
                                       vk::BufferUsageFlagBits::eUniformBuffer,
                                       vma::MemoryUsage::eCpuToGpu);

    // allocate ssbo buffer for object data
    const int MAX_OBJECTS = 10000;
    frame.objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS,
                                       vk::BufferUsageFlagBits::eStorageBuffer,
                                       vma::MemoryUsage::eCpuToGpu);

    // allocate descriptor set
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(m_descriptorPool);
    allocInfo.setSetLayouts(m_globalSetLayout);
    frame.globalDescriptor = m_device.allocateDescriptorSets(allocInfo)[0];

    // point it to camera buffer
    vk::DescriptorBufferInfo cameraInfo;
    cameraInfo.setBuffer(frame.cameraBuffer.buffer);
    cameraInfo.setOffset(0);
    cameraInfo.setRange(sizeof(GPUCameraData));

    vk::DescriptorBufferInfo sceneInfo;
    sceneInfo.setBuffer(m_sceneParameterBuffer.buffer);
    sceneInfo.setOffset(0);
    sceneInfo.setRange(sizeof(GPUSceneData));

    vk::DescriptorBufferInfo objectInfo;
    objectInfo.setBuffer(frame.objectBuffer.buffer);
    objectInfo.setOffset(0);
    objectInfo.setRange(sizeof(GPUObjectData) * MAX_OBJECTS);

    // write to binding 0
    vk::WriteDescriptorSet cameraWrite(frame.globalDescriptor, 0, 0, 1,
                                       vk::DescriptorType::eUniformBuffer,
                                       nullptr, &cameraInfo, nullptr);
    vk::WriteDescriptorSet sceneWrite(frame.globalDescriptor, 1, 0, 1,
                                      vk::DescriptorType::eUniformBufferDynamic,
                                      nullptr, &sceneInfo, nullptr);
    vk::WriteDescriptorSet objectWrite(frame.objectDescriptor, 0, 0, 1,
                                       vk::DescriptorType::eStorageBuffer,
                                       nullptr, &objectInfo, nullptr);
    vk::WriteDescriptorSet writes[] = {cameraWrite, sceneWrite, objectWrite};
    m_device.updateDescriptorSets(3, writes, 0, nullptr);

    m_mainDeletionQueue.push_function([&]() {
      m_allocator.destroyBuffer(frame.cameraBuffer.buffer,
                                frame.cameraBuffer.allocation);
      m_allocator.destroyBuffer(frame.objectBuffer.buffer,
                                frame.objectBuffer.allocation);
    });
    i++;
  }
}

void VulkanEngine::init_pipelines() {
  PipelineBuilder pipelineBuilder;

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

  vk::PipelineLayoutCreateInfo meshPipelineLayoutInfo =
      PipelineBuilder::default_pipeline_layout_create_info();
  // push constant setup
  vk::PushConstantRange pushConstant;
  pushConstant.setSize(sizeof(MeshPushConstants))
      .setOffset(0)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex);
  meshPipelineLayoutInfo.setPushConstantRanges(pushConstant);
  // setup global descriptor set layout
  vk::DescriptorSetLayout setLayouts[] = {m_globalSetLayout, m_objectSetLayout};
  meshPipelineLayoutInfo.setSetLayouts(setLayouts);

  m_meshPipelineLayout = m_device.createPipelineLayout(meshPipelineLayoutInfo);

  pipelineBuilder.shaderStages.clear();
  VertexInputDescription vertexDescription = Vertex::get_vertex_description();
  pipelineBuilder.vertexInputInfo.setVertexAttributeDescriptions(
      vertexDescription.attributes);
  pipelineBuilder.vertexInputInfo.setVertexBindingDescriptions(
      vertexDescription.bindings);
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex,
          m_shaderModules["basic_normalcolor_mesh.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment,
          m_shaderModules["basic_flat_mesh.frag"]));
  pipelineBuilder.pipelineLayout = m_meshPipelineLayout;
  pipelineBuilder.depthStencil =
      PipelineBuilder::default_depth_stencil_create_info(
          true, true, vk::CompareOp::eLessOrEqual);
  m_meshPipeline = pipelineBuilder.build(m_device, m_renderPass);
  create_material(m_meshPipeline, m_meshPipelineLayout, "defaultmesh");

  m_mainDeletionQueue.push_function([=]() {
    m_device.destroyPipeline(m_meshPipeline);
    m_device.destroyPipelineLayout(m_meshPipelineLayout);
  });

  spdlog::info("Finished building mesh triangle pipeline");
}

// TODO: move this to a utils section later
AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize,
                                            vk::BufferUsageFlags usage,
                                            vma::MemoryUsage memoryUsage) {
  vk::BufferCreateInfo bufferInfo;
  bufferInfo.setSize(allocSize);
  bufferInfo.setUsage(usage);

  vma::AllocationCreateInfo vmaallocInfo;
  vmaallocInfo.setUsage(memoryUsage);

  AllocatedBuffer newBuffer;

  auto allocated = m_allocator.createBuffer(bufferInfo, vmaallocInfo);
  newBuffer.buffer = allocated.first;
  newBuffer.allocation = allocated.second;

  return newBuffer;
}
} // namespace vkr