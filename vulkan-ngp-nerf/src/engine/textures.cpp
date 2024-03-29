#include "textures.hpp"
#include "common_includes.h"
#include "engine.hpp"

// implementation already defined in libngp static library
// #define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

namespace vkr {
std::shared_ptr<Texture>
VulkanEngine::load_image_texture_from_file(const std::string &path) {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels,
                              STBI_rgb_alpha);
  if (!pixels) {
    spdlog::error("Failed to load image file: {}", path);
    return {};
  }

  void *pixel_ptr = pixels;
  // TODO: make this use texChannels?
  vk::DeviceSize imageSize = texWidth * texHeight * 4;
  vk::Format imageFormat = vk::Format::eR8G8B8A8Srgb;

  // TODO: abstract into staging copy
  AllocatedBuffer stagingBuffer = create_buffer(
      imageSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto,
      vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

  void *data = m_allocator.mapMemory(stagingBuffer.allocation);
  memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
  m_allocator.unmapMemory(stagingBuffer.allocation);

  stbi_image_free(pixels);

  std::shared_ptr<Texture> texture = createEmptyDefaultTexture(
      texWidth, texHeight, imageFormat, vk::ImageLayout::eTransferDstOptimal,
      vk::AccessFlagBits::eTransferWrite,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::PipelineStageFlagBits::eTransfer);

  // image data is now in the staging buffer, now setup gpu receiver image
  // transition image layout
  immediate_submit([&](vk::CommandBuffer cmd) {
    vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                    1);
    // now do the actual copy from the staging buffer to the gpu image
    vk::BufferImageCopy copyRegion(0, 0, 0);
    copyRegion.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
    copyRegion.imageSubresource.setMipLevel(0);
    copyRegion.imageSubresource.setBaseArrayLayer(0);
    copyRegion.imageSubresource.setLayerCount(1);
    copyRegion.setImageExtent(texture->imageExtent);

    cmd.copyBufferToImage(stagingBuffer.buffer, texture->image.image,
                          vk::ImageLayout::eTransferDstOptimal, copyRegion);

    // now transform image to shader optimal reading
    vk::ImageMemoryBarrier imageBarrier_toReadable;
    imageBarrier_toReadable.setImage(texture->image.image);
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
  });

  m_allocator.destroyBuffer(stagingBuffer.buffer, stagingBuffer.allocation);

  spdlog::info("Loaded texture successfully from: {}", path);

  return texture;
}

void VulkanEngine::load_images() {
  m_loadedTextures["empire_diffuse"] =
      load_image_texture_from_file("../assets/models/viking_room.png");
}

std::shared_ptr<Texture> VulkanEngine::createEmptyDefaultTexture(
    int texWidth, int texHeight, vk::Format imageFormat,
    vk::ImageLayout imageLayout, vk::Flags<vk::AccessFlagBits> imageAccess,
    vk::Flags<vk::ImageUsageFlagBits> usage,
    vk::Flags<vk::PipelineStageFlagBits> setupCompletionPipelineStage) {
  vk::Extent3D imageExtent;
  imageExtent.setWidth(texWidth);
  imageExtent.setHeight(texHeight);
  imageExtent.setDepth(1);

  vk::ImageCreateInfo imageInfo;
  imageInfo.setFormat(imageFormat);
  imageInfo.setUsage(usage);
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

  // transition to wanted image layout
  immediate_submit([&](vk::CommandBuffer cmd) {
    vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                    1);
    vk::ImageMemoryBarrier imageBarrier_toTransfer;
    imageBarrier_toTransfer.setOldLayout(vk::ImageLayout::eUndefined);
    imageBarrier_toTransfer.setNewLayout(imageLayout);
    imageBarrier_toTransfer.setImage(newImage.image);
    imageBarrier_toTransfer.setSubresourceRange(range);
    imageBarrier_toTransfer.setSrcAccessMask({});
    imageBarrier_toTransfer.setDstAccessMask(imageAccess);
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        setupCompletionPipelineStage, {}, nullptr, nullptr,
                        imageBarrier_toTransfer);
  });

  m_mainDeletionQueue.push_function(
      [&]() { m_allocator.destroyImage(newImage.image, newImage.allocation); });

  std::shared_ptr<Texture> newTexture = std::make_shared<Texture>();
  newTexture->image = newImage;
  newTexture->imageExtent = imageExtent;
  vk::ImageViewCreateInfo imageViewCreate;
  imageViewCreate.setImage(newTexture->image.image);
  imageViewCreate.setFormat(imageFormat);
  imageViewCreate.setViewType(vk::ImageViewType::e2D);
  imageViewCreate.subresourceRange.setAspectMask(
      vk::ImageAspectFlagBits::eColor);
  imageViewCreate.subresourceRange.setBaseMipLevel(0);
  imageViewCreate.subresourceRange.setLevelCount(1);
  imageViewCreate.subresourceRange.setBaseArrayLayer(0);
  imageViewCreate.subresourceRange.setLayerCount(1);
  newTexture->imageView = m_device.createImageView(imageViewCreate);

  m_mainDeletionQueue.push_function(
      [&] { m_device.destroyImageView(newTexture->imageView); });

  return newTexture;
}
} // namespace vkr