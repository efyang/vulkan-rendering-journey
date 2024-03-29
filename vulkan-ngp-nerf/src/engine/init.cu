// SDL
#include "SDL_events.h"
#include "SDL_scancode.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_vulkan.h>

// vkb
#include "VkBootstrap.h"

#include "common_includes.h"

// ngp
#include <neural-graphics-primitives/testbed.h>
#include <neural-graphics-primitives/dlss.h>
#include <neural-graphics-primitives/common.h>
#include <neural-graphics-primitives/render_buffer.h>
#include <ngp.hpp>

// ngx
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

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
  init_shader_modules();
  init_descriptors();
  init_pipelines();
  load_meshes();
  load_images();
  init_ngp();
  init_scene();

  // everything went fine
  m_isInitialized = true;
}

void VulkanEngine::init_vulkan() {
  uint32_t n_ngx_instance_extensions = 0;
  const char** ngx_instance_extensions;

  uint32_t n_ngx_device_extensions = 0;
  const char** ngx_device_extensions;

  NVSDK_NGX_VULKAN_RequiredExtensions(&n_ngx_instance_extensions, &ngx_instance_extensions, &n_ngx_device_extensions, &ngx_device_extensions);

  vkb::InstanceBuilder instanceBuilder;
  auto vkb_instbuilder = instanceBuilder.set_app_name(m_appName.c_str())
                      .request_validation_layers(true)
                      .require_api_version(1, 3, 0)
                      .use_default_debug_messenger()
                      .enable_extension(VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME)
                      .enable_extension(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME)
                      .enable_extension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)
                      .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  // std::cout << "enabling instance extensions" << std::endl;
	// for (uint32_t i = 0; i < n_ngx_instance_extensions; ++i) {
  //   std::cout << ngx_instance_extensions[i] << std::endl;
	// 	vkb_instbuilder.enable_extension(ngx_instance_extensions[i]);
	// } 
  auto vkb_inst = vkb_instbuilder
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
  vk::PhysicalDeviceFeatures features;
  features.shaderStorageImageWriteWithoutFormat = true;
  vk::PhysicalDeviceVulkan13Features features_13;
  features_13.dynamicRendering = true;
  selector = selector.set_minimum_version(1, 3)
                 .set_surface(m_surface)
                 .add_required_extension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)
                 .add_required_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)
                 .add_required_extension(VK_KHR_DEVICE_GROUP_EXTENSION_NAME)
                 .add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
                 .add_required_extension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
                 .set_required_features(features)
                 .set_required_features_13(
                     static_cast<VkPhysicalDeviceVulkan13Features>(features_13))
      /*
      .add_required_extension("VK_KHR_acceleration_structure")
      .add_required_extension("VK_KHR_ray_tracing_pipeline")
      .add_required_extension("VK_NV_mesh_shader")
      */
      ;

  // std::cout << "enabling device extensions" << std::endl;
	// for (uint32_t i = 0; i < n_ngx_device_extensions; ++i) {
  //   std::cout << ngx_device_extensions[i] << std::endl;
	// 	selector = selector.add_required_extension(ngx_device_extensions[i]);
	// }

  // TODO: copy over physical device selection code based on cuda from dlss.cu

  auto availableDevices = selector.select_device_names();
  uint32_t idx = 0;
  for (std::string name : availableDevices.value()) {
    spdlog::info("Available physical device {}: {}", idx++, name);
  }

  vkb::PhysicalDevice physicalDevice = selector.set_name("NVIDIA GeForce RTX 3070 Laptop GPU").select().value();
  vkb::DeviceBuilder deviceBuilder(physicalDevice);
  vk::PhysicalDeviceShaderDrawParametersFeatures
      shader_draw_parameters_features(true);
  vk::PhysicalDeviceBufferDeviceAddressFeaturesEXT buffer_device_address_feature(true);
  vkb::Device vkbDevice =
      deviceBuilder.add_pNext(&shader_draw_parameters_features)
        .add_pNext(&buffer_device_address_feature).build().value();

  m_device = vkbDevice.device;
  m_physicalDevice = vkbDevice.physical_device;
  m_gpuProperties = vkbDevice.physical_device.properties;

	struct QueueFamilyIndices {
		int graphics_family = -1;
		int compute_family = -1;
		int transfer_family = -1;
		int all_family = -1;
	};

	auto find_queue_families = [](VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

		int i = 0;
		for (const auto& queue_family : queue_families) {
			if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphics_family = i;
			}

			if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				indices.compute_family = i;
			}

			if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				indices.transfer_family = i;
			}

			if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) && (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
				indices.all_family = i;
			}

			i++;
		}

		return indices;
	};



	cudaDeviceProp cuda_device_prop;
	CUDA_CHECK_THROW(cudaGetDeviceProperties(&cuda_device_prop, tcnn::cuda_device()));

	auto is_same_as_cuda_device = [&](VkPhysicalDevice device) {
		VkPhysicalDeviceIDProperties physical_device_id_properties = {};
		physical_device_id_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
		physical_device_id_properties.pNext = NULL;

		VkPhysicalDeviceProperties2 physical_device_properties = {};
		physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		physical_device_properties.pNext = &physical_device_id_properties;

		vkGetPhysicalDeviceProperties2(device, &physical_device_properties);

		return !memcmp(&cuda_device_prop.uuid, physical_device_id_properties.deviceUUID, VK_UUID_SIZE) && find_queue_families(device).all_family >= 0;
	};

  if (is_same_as_cuda_device(m_physicalDevice)) {
    std::cout << "vulkan cuda device okay" << std::endl;
  } else {
    std::cout << "vulkan cuda device will fail" << std::endl;
  }

  spdlog::info("Initialized physical device: {}",
               std::string(m_physicalDevice.getProperties().deviceName));
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
  // create upload command pool
  vk::CommandPoolCreateFlags uploadCommandPoolCreateFlags;
  uploadCommandPoolCreateFlags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  vk::CommandPoolCreateInfo uploadCommandPoolInfo(uploadCommandPoolCreateFlags,
                                                  m_graphicsQueueFamily);
  m_uploadContext.commandPool =
      m_device.createCommandPool(uploadCommandPoolInfo);
  m_mainDeletionQueue.push_function(
      [&] { m_device.destroyCommandPool(m_uploadContext.commandPool); });

  vk::CommandBufferAllocateInfo instantCommandBufferAllocateInfo(
      m_uploadContext.commandPool, vk::CommandBufferLevel::ePrimary, 1);
  m_uploadContext.commandBuffer =
      m_device.allocateCommandBuffers(instantCommandBufferAllocateInfo)[0];

  m_ngpUploadContext.commandPool =
      m_device.createCommandPool(uploadCommandPoolInfo);
  m_mainDeletionQueue.push_function(
      [&] { m_device.destroyCommandPool(m_ngpUploadContext.commandPool); });
  m_ngpUploadContext.commandBuffer =
      m_device.allocateCommandBuffers(instantCommandBufferAllocateInfo)[0];

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
        frame.m_commandPool, vk::CommandBufferLevel::ePrimary, 1);
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
  // create so that the fence is default signaled
  vk::FenceCreateInfo fenceCreateInfo;
  fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

  vk::FenceCreateInfo uploadFenceCreateInfo;
  m_uploadContext.uploadFence = m_device.createFence(uploadFenceCreateInfo);

  m_mainDeletionQueue.push_function(
      [=]() { m_device.destroyFence(m_uploadContext.uploadFence); });

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
      {vk::DescriptorType::eUniformBufferDynamic, 10},
      {vk::DescriptorType::eStorageBuffer, 10},
      {vk::DescriptorType::eStorageBuffer, 10},
      {vk::DescriptorType::eCombinedImageSampler, 10}};

  vk::DescriptorPoolCreateInfo poolInfo;
  poolInfo.setMaxSets(10);
  poolInfo.setPoolSizes(sizes);

  m_descriptorPool = m_device.createDescriptorPool(poolInfo, nullptr);

  // camera data at 0
  vk::DescriptorSetLayoutBinding camBufferBinding;
  camBufferBinding.setBinding(0);
  camBufferBinding.setDescriptorCount(1);
  camBufferBinding.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic);
  camBufferBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
  // scene data at 1
  vk::DescriptorSetLayoutBinding sceneBind(
      1, vk::DescriptorType::eUniformBufferDynamic, 1,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

  vk::DescriptorSetLayoutBinding bindings[] = {camBufferBinding, sceneBind};

  vk::DescriptorSetLayoutCreateInfo setInfo;
  setInfo.setBindings(bindings);
  m_globalSetLayout = m_device.createDescriptorSetLayout(setInfo, nullptr);

  // allocate global descriptor set
  vk::DescriptorSetAllocateInfo allocInfo;
  allocInfo.setDescriptorPool(m_descriptorPool);
  allocInfo.setSetLayouts(m_globalSetLayout);
  m_globalDescriptorSet = m_device.allocateDescriptorSets(allocInfo)[0];
  // allocate camera scene buffer
  uint32_t cameraSceneBufferSize =
      FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUCameraSceneData));
  m_cameraSceneBuffer = create_buffer(
      cameraSceneBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
      vma::MemoryUsage::eAuto,
      vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
  m_mainDeletionQueue.push_function([&] {
    m_allocator.destroyBuffer(m_cameraSceneBuffer.buffer,
                              m_cameraSceneBuffer.allocation);
  });

  // point camera and scene buffers to the combined buffer
  vk::DescriptorBufferInfo cameraInfo;
  cameraInfo.setBuffer(m_cameraSceneBuffer.buffer);
  cameraInfo.setOffset(offsetof(GPUCameraSceneData, cameraData));
  cameraInfo.setRange(sizeof(GPUCameraData));
  vk::DescriptorBufferInfo sceneInfo;
  sceneInfo.setBuffer(m_cameraSceneBuffer.buffer);
  sceneInfo.setOffset(offsetof(GPUCameraSceneData, sceneData));
  sceneInfo.setRange(sizeof(GPUSceneData));

  // per-object bindings
  vk::DescriptorSetLayoutBinding objectBind(
      0, vk::DescriptorType::eStorageBuffer, 1,
      vk::ShaderStageFlagBits::eVertex);
  vk::DescriptorSetLayoutBinding objectLightingBind(
      1, vk::DescriptorType::eStorageBuffer, 1,
      vk::ShaderStageFlagBits::eVertex);
  vk::DescriptorSetLayoutBinding objectBindings[] = {objectBind,
                                                     objectLightingBind};
  vk::DescriptorSetLayoutCreateInfo objectSetInfo;
  objectSetInfo.setBindings(objectBindings);
  m_objectSetLayout = m_device.createDescriptorSetLayout(objectSetInfo);

  // single texture set
  vk::DescriptorSetLayoutBinding textureBind(
      0, vk::DescriptorType::eCombinedImageSampler, 1,
      vk::ShaderStageFlagBits::eFragment);

  vk::DescriptorSetLayoutCreateInfo textureSetInfo;
  textureSetInfo.setBindings(textureBind);
  m_singleTextureSetLayout = m_device.createDescriptorSetLayout(textureSetInfo);

  m_mainDeletionQueue.push_function([&]() {
    m_device.destroyDescriptorSetLayout(m_globalSetLayout);
    m_device.destroyDescriptorSetLayout(m_objectSetLayout);
    m_device.destroyDescriptorSetLayout(m_singleTextureSetLayout);
    m_device.destroyDescriptorPool(m_descriptorPool);
  });

  size_t i = 0;
  for (FrameData &frame : m_frames) {
    vk::DescriptorSetAllocateInfo objectSetAlloc;
    objectSetAlloc.setDescriptorPool(m_descriptorPool);
    objectSetAlloc.setSetLayouts(m_objectSetLayout);
    frame.objectDescriptorSet =
        m_device.allocateDescriptorSets(objectSetAlloc)[0];

    // allocate ssbo buffer for object data
    const int MAX_OBJECTS = 10000;
    frame.objectBuffer = create_buffer(
        sizeof(GPUObjectData) * MAX_OBJECTS,
        vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAuto,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

    vk::DescriptorBufferInfo objectInfo;
    objectInfo.setBuffer(frame.objectBuffer.buffer);
    objectInfo.setOffset(0);
    objectInfo.setRange(sizeof(GPUObjectData) * MAX_OBJECTS);

    frame.objectLightingBuffer = create_buffer(
        sizeof(GPUObjectLightingData) * MAX_OBJECTS,
        vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAuto,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
    vk::DescriptorBufferInfo objectLightingInfo;
    objectLightingInfo.setBuffer(frame.objectLightingBuffer.buffer);
    objectLightingInfo.setOffset(0);
    objectLightingInfo.setRange(sizeof(GPUObjectLightingData) * MAX_OBJECTS);

    // write to binding 0
    vk::WriteDescriptorSet cameraWrite(
        m_globalDescriptorSet, 0, 0, 1,
        vk::DescriptorType::eUniformBufferDynamic, nullptr, &cameraInfo,
        nullptr);
    vk::WriteDescriptorSet sceneWrite(m_globalDescriptorSet, 1, 0, 1,
                                      vk::DescriptorType::eUniformBufferDynamic,
                                      nullptr, &sceneInfo, nullptr);
    vk::WriteDescriptorSet objectWrite(frame.objectDescriptorSet, 0, 0, 1,
                                       vk::DescriptorType::eStorageBuffer,
                                       nullptr, &objectInfo, nullptr);
    vk::WriteDescriptorSet objectLightingWrite(
        frame.objectDescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer,
        nullptr, &objectLightingInfo, nullptr);
    vk::WriteDescriptorSet writes[] = {cameraWrite, sceneWrite, objectWrite,
                                       objectLightingWrite};
    m_device.updateDescriptorSets(std::size(writes), writes, 0, nullptr);

    m_mainDeletionQueue.push_function([&]() {
      m_allocator.destroyBuffer(frame.objectBuffer.buffer,
                                frame.objectBuffer.allocation);
      m_allocator.destroyBuffer(frame.objectLightingBuffer.buffer,
                                frame.objectLightingBuffer.allocation);
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

  vk::DescriptorSetLayout setLayouts[] = {m_globalSetLayout, m_objectSetLayout};
  meshPipelineLayoutInfo.setSetLayouts(setLayouts);
  m_meshPipelineLayout = m_device.createPipelineLayout(meshPipelineLayoutInfo);

  // setup textured pipeline layout
  vk::PipelineLayoutCreateInfo texturedPipelineCreateInfo =
      meshPipelineLayoutInfo;
  vk::DescriptorSetLayout texturedSetLayouts[] = {
      m_globalSetLayout, m_objectSetLayout, m_singleTextureSetLayout};
  texturedPipelineCreateInfo.setSetLayouts(texturedSetLayouts);
  vk::PipelineLayout texturedPipelineLayout =
      m_device.createPipelineLayout(texturedPipelineCreateInfo);

  vk::PipelineLayout nerfTexturedPipelineLayout =
      m_device.createPipelineLayout(texturedPipelineCreateInfo);

  // default vertices
  VertexInputDescription vertexDescription = Vertex::get_vertex_description();
  pipelineBuilder.vertexInputInfo.setVertexAttributeDescriptions(
      vertexDescription.attributes);
  pipelineBuilder.vertexInputInfo.setVertexBindingDescriptions(
      vertexDescription.bindings);

  // default depth
  pipelineBuilder.pipelineLayout = m_meshPipelineLayout;
  pipelineBuilder.depthStencil =
      PipelineBuilder::default_depth_stencil_create_info(
          true, true, vk::CompareOp::eLessOrEqual);

  // create default mesh pipeline
  pipelineBuilder.shaderStages.clear();
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex,
          m_shaderModules["basic_normalcolor_mesh.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment,
          m_shaderModules["basic_flat_mesh.frag"]));

  m_meshPipeline = pipelineBuilder.build(m_device, m_renderPass);
  create_material(m_meshPipeline, m_meshPipelineLayout, "defaultmesh");

  // create pipeline for textured drawing
  pipelineBuilder.pipelineLayout = texturedPipelineLayout;
  pipelineBuilder.shaderStages.clear();
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex,
          m_shaderModules["basic_normalcolor_mesh.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment,
          m_shaderModules["textured_lit.frag"]));
  vk::Pipeline texPipeline = pipelineBuilder.build(m_device, m_renderPass);
  create_material(texPipeline, texturedPipelineLayout, "texturedmesh");

  // nerf pipeline
  pipelineBuilder.pipelineLayout = nerfTexturedPipelineLayout;
  pipelineBuilder.shaderStages.clear();
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eVertex,
          m_shaderModules["basic_normalcolor_mesh.vert"]));
  pipelineBuilder.shaderStages.push_back(
      PipelineBuilder::default_pipeline_shader_stage_create_info(
          vk::ShaderStageFlagBits::eFragment,
          m_shaderModules["nerf_ss.frag"]));
  vk::Pipeline nerfPipeline = pipelineBuilder.build(m_device, m_renderPass);
  create_material(nerfPipeline, nerfTexturedPipelineLayout, "nerfmesh");



  //

  m_mainDeletionQueue.push_function([=]() {
    m_device.destroyPipeline(m_meshPipeline);
    m_device.destroyPipeline(texPipeline);
    m_device.destroyPipeline(nerfPipeline);
    m_device.destroyPipelineLayout(m_meshPipelineLayout);
    m_device.destroyPipelineLayout(texturedPipelineLayout);
  });

  spdlog::info("Finished building mesh triangle pipeline");
}

// according to new reference docs, usage should be one of the AUTO enumerations
// only, use flags to specify instead
// TODO: move this to a utils section later
AllocatedBuffer
VulkanEngine::create_buffer(size_t allocSize, vk::BufferUsageFlags usage,
                            vma::MemoryUsage memoryUsage,
                            vma::AllocationCreateFlags memoryFlags) {
  vk::BufferCreateInfo bufferInfo;
  bufferInfo.setSize(allocSize);
  bufferInfo.setUsage(usage);

  vma::AllocationCreateInfo vmaallocInfo;
  vmaallocInfo.setUsage(memoryUsage);
  vmaallocInfo.setFlags(memoryFlags);

  AllocatedBuffer newBuffer;

  auto allocated = m_allocator.createBuffer(bufferInfo, vmaallocInfo);
  newBuffer.buffer = allocated.first;
  newBuffer.allocation = allocated.second;

  return newBuffer;
}

void VulkanEngine::init_ngp() {
    ngp::init_ngp_vulkan_manual(
        m_instance,
        VK_NULL_HANDLE,
	    m_physicalDevice,
		m_device,
		m_graphicsQueue,
		m_ngpUploadContext.commandPool,
		m_ngpUploadContext.commandBuffer);

  // ------------------------------ NGP test code -------------------------------------
  nerfRO = std::make_shared<NerfRenderObject>(192 * 3, 108 * 3, "../instant-ngp/data/nerf/fox/base.msgpack", *this);
  // nerfRO = (void *)newNerfRenderObject(1920, 1080, "../instant-ngp/data/nerf/fox/base.msgpack", *this);
  Eigen::Matrix<float, 3, 4> camera_matrix;
  // taken from basic init of instant-ngp testbed
  camera_matrix << 1, 0, 0, 0.5,
        0, -1,  0, 0.5,
        0,  0, -1,   2;
  std::static_pointer_cast<NerfRenderObject>(nerfRO)->update(camera_matrix, *this);
  // this->nerfRO = std::static_pointer_cast<void>(nerfRO);
  // initialize a testbed
  /*
  ngp::Testbed testbed(ngp::ETestbedMode::Nerf);
  // load the fox snapshot
  testbed.load_snapshot("../instant-ngp/data/nerf/fox/base.msgpack");
  
  // create an Eigen::vector
  Eigen::Matrix<float, 3, 4> camera_matrix;
  // camera_matrix << 1, 0, 0, 0,
  //      0, 1, 0, 0,
  //      0, 0, 1, 0;
  // taken from basic init of instant-ngp testbed
  camera_matrix << 1, 0, 0, 0.5,
        0, -1,  0, 0.5,
        0,  0, -1,   2;
  std::cout << camera_matrix;
  Eigen::Vector4f rolling_shutter = Eigen::Vector4f::Zero();

  // create a vulkan texture
  std::shared_ptr<ngp::VulkanTextureSurface> vktex = std::make_shared<ngp::VulkanTextureSurface>(Eigen::Vector2i(1920, 1080), 4);
  ngp::CudaRenderBuffer vkRenderSurface(vktex);
  vkRenderSurface.resize(Eigen::Vector2i(1920, 1080));

  spdlog::info("Rendering nerf frame... ");
  testbed.render_frame(camera_matrix, camera_matrix, rolling_shutter, vkRenderSurface);
  spdlog::info("Finished rendering nerf frame.");

  vk::Image ngpRawImage(vktex->vk_image());

  spdlog::info("Creating new image to hold nerf texture...");
// create new image
  int texWidth = 1920;
  int texHeight = 1080;
  vk::Extent3D imageExtent;
  imageExtent.setWidth(texWidth);
  imageExtent.setHeight(texHeight);
  imageExtent.setDepth(1);

  vk::DeviceSize imageSize = texWidth * texHeight * 4;
  vk::Format imageFormat = vk::Format::eR8G8B8A8Srgb;

  vk::ImageCreateInfo imageInfo;
  imageInfo.setFormat(imageFormat);
  imageInfo.setUsage(vk::ImageUsageFlagBits::eTransferDst |
                     vk::ImageUsageFlagBits::eSampled);
  imageInfo.setExtent(imageExtent);
  imageInfo.setMipLevels(1);
  imageInfo.setArrayLayers(1);
  imageInfo.setSamples(vk::SampleCountFlagBits::e1);
  imageInfo.setTiling(vk::ImageTiling::eOptimal);
  imageInfo.setImageType(vk::ImageType::e2D);

  AllocatedImage newImage;
  vma::AllocationCreateInfo imageAllocInfo;
  imageAllocInfo.setUsage(vma::MemoryUsage::eGpuOnly);
  std::pair<vk::Image, vma::Allocation> allocedImage =
      m_allocator.createImage(imageInfo, imageAllocInfo);
  newImage.image = allocedImage.first;
  newImage.allocation = allocedImage.second;
  spdlog::info("Allocated new image to hold nerf texture.");

  // transition image layout
  immediate_submit([&](vk::CommandBuffer cmd) {
    vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                    1);

// keep this
    vk::ImageMemoryBarrier imageBarrier_toTransfer;
    imageBarrier_toTransfer.setOldLayout(vk::ImageLayout::eUndefined);
    imageBarrier_toTransfer.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
    imageBarrier_toTransfer.setImage(newImage.image);
    imageBarrier_toTransfer.setSubresourceRange(range);
    imageBarrier_toTransfer.setSrcAccessMask({});
    imageBarrier_toTransfer.setDstAccessMask(
        vk::AccessFlagBits::eTransferWrite);
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eTransfer, {}, nullptr,
                        nullptr, imageBarrier_toTransfer);

    // do our blit
    vk::ImageBlit blitRegion;
    blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcOffsets[0].x = 0;
    blitRegion.srcOffsets[0].y = 0;
    blitRegion.srcOffsets[0].z = 0;
    blitRegion.srcOffsets[1].x = texWidth;
    blitRegion.srcOffsets[1].y = texHeight;
    blitRegion.srcOffsets[1].z = 1;
    blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstOffsets[0] = blitRegion.srcOffsets[0];
    blitRegion.dstOffsets[1] = blitRegion.srcOffsets[1];

    cmd.blitImage(ngpRawImage,
                  vk::ImageLayout::eGeneral,
                  newImage.image,
                  vk::ImageLayout::eTransferDstOptimal,
                  blitRegion,
                  vk::Filter::eNearest);
  });

  m_mainDeletionQueue.push_function(
      [=]() { m_allocator.destroyImage(newImage.image, newImage.allocation); });



  Texture ngpnerf;
  ngpnerf.image = newImage;

  immediate_submit([&](vk::CommandBuffer cmd) {
    vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                    1);
    // now transform image to shader optimal reading
    vk::ImageMemoryBarrier imageBarrier_toReadable;
    imageBarrier_toReadable.setImage(ngpnerf.image.image);
    imageBarrier_toReadable.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    imageBarrier_toReadable.setNewLayout(
        vk::ImageLayout::eShaderReadOnlyOptimal);
    imageBarrier_toReadable.setSubresourceRange(range);
    imageBarrier_toReadable.setSrcAccessMask(
        vk::AccessFlagBits::eTransferWrite);
    imageBarrier_toReadable.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr,
                        nullptr, imageBarrier_toReadable);
    spdlog::info("Converted image successfully");
  });

  vk::ImageViewCreateInfo imageViewCreate;
  imageViewCreate.setImage(ngpnerf.image.image);
  imageViewCreate.setFormat(vk::Format::eR8G8B8A8Srgb);
  imageViewCreate.setViewType(vk::ImageViewType::e2D);
  imageViewCreate.subresourceRange.setAspectMask(
      vk::ImageAspectFlagBits::eColor);
  imageViewCreate.subresourceRange.setBaseMipLevel(0);
  imageViewCreate.subresourceRange.setLevelCount(1);
  imageViewCreate.subresourceRange.setBaseArrayLayer(0);
  imageViewCreate.subresourceRange.setLayerCount(1);
  ngpnerf.imageView = m_device.createImageView(imageViewCreate);
  // image is getting freed - we probably need to blit it over

  m_loadedTextures["ngpnerf"] = ngpnerf;

  m_mainDeletionQueue.push_function(
      [=] { m_device.destroyImageView(ngpnerf.imageView); });
*/
}
} // namespace vkr