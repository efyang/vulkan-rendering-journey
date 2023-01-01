#include "ngp.hpp"

namespace vkr {
	NerfRenderObject::NerfRenderObject(int texWidth, int texHeight, const std::string snapshotPath, VulkanEngine& engine):
		texWidth(texWidth), texHeight(texHeight) {
    aabbMesh = std::make_shared<Mesh>();
    renderObject = std::make_shared<RenderObject>();
    // load the snapshot
    testbed.load_snapshot(snapshotPath);

    // build centered cube mesh
    bbCenter = (testbed.m_aabb.min + testbed.m_aabb.max) * 0.5;
    Eigen::Vector3f bbMin = testbed.m_aabb.min - bbCenter;
    Eigen::Vector3f bbMax = testbed.m_aabb.max - bbCenter;
    // TODO: add free for renderObject.mesh in deletion queue

    (aabbMesh->vertices).resize(8);
    (aabbMesh->vertices)[0].position = {bbMin.x(), bbMin.y(), bbMin.z()};
    (aabbMesh->vertices)[1].position = {bbMin.x(), bbMax.y(), bbMin.z()};
    (aabbMesh->vertices)[2].position = {bbMax.x(), bbMin.y(), bbMin.z()};
    (aabbMesh->vertices)[3].position = {bbMax.x(), bbMax.y(), bbMin.z()};
    (aabbMesh->vertices)[4].position = {bbMax.x(), bbMax.y(), bbMax.z()};
    (aabbMesh->vertices)[5].position = {bbMax.x(), bbMin.y(), bbMax.z()};
    (aabbMesh->vertices)[6].position = {bbMin.x(), bbMin.y(), bbMax.z()};
    (aabbMesh->vertices)[7].position = {bbMin.x(), bbMax.y(), bbMax.z()};

    (aabbMesh->vertices)[0].uv = {0., 0.};
    (aabbMesh->vertices)[1].uv = {1., 0.};
    (aabbMesh->vertices)[2].uv = {0., 1.};
    (aabbMesh->vertices)[3].uv = {1., 1.};
    (aabbMesh->vertices)[4].uv = {0., 0.};
    (aabbMesh->vertices)[5].uv = {1., 0.};
    (aabbMesh->vertices)[6].uv = {0., 1.};
    (aabbMesh->vertices)[7].uv = {1., 1.};

    for (int i = 0; i < (aabbMesh->vertices).size(); i++) {
      (aabbMesh->vertices)[i].color = {1.0, 1.0, 1.0};
    }

    aabbMesh->indices = {1,0,2,
                      2,3,1,
                      4,3,2,
                      2,5,4,
                      7,4,5,
                      5,6,7,
                      7,6,0,
                      0,1,7,
                      1,3,4,
                      4,7,1,
                      2,0,6,
                      6,5,2};

    // this address can change with new meshes which is why it breaks?
    // or maybe changes on accesses?
    // engine.upload_mesh(engine.m_meshes[snapshotPath + "aabbMesh"]);
    engine.upload_mesh(*aabbMesh);
    engine.m_meshes[snapshotPath + "aabbMesh"] = *aabbMesh;
    // renderObject.mesh = &aabbMesh;
    // renderObject.mesh = &engine.get_mesh(snapshotPath + "aabbMesh");

    // spdlog::info("vsize {}, isize {}", renderObject.mesh->vertices.size(), renderObject.mesh->indices.size());
    cameraMatrix << 1, 0, 0, 0.5,
        0, -1,  0, 0.5,
        0,  0, -1,   2;
    prevCameraMatrix = cameraMatrix;

    // create a vulkan texture
    rawVkImage = std::make_shared<ngp::VulkanTextureSurface>(Eigen::Vector2i(texWidth, texHeight), 4);
    cudaRenderBuffer = std::make_unique<ngp::CudaRenderBuffer>(rawVkImage);
    cudaRenderBuffer->resize(Eigen::Vector2i(texWidth, texHeight));

    // default to shader readable
    engine.m_loadedTextures["nerf"] = engine.createEmptyDefaultTexture(
      texWidth, texHeight,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::AccessFlagBits::eShaderRead,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::PipelineStageFlagBits::eFragmentShader);
	}

  void NerfRenderObject::update(Eigen::Matrix<float, 3, 4> camera_matrix, VulkanEngine& engine) {
    // AKA clear the frame
    cudaRenderBuffer->reset_accumulation();
    vk::Image ngpRawImage(rawVkImage->vk_image());

    // render nerf to the rawvktexture
    // Eigen::Affine3f centerShift(-bbCenter);
    // camera_matrix = camera_matrix * centerShift.matrix();
    Eigen::Vector4f rolling_shutter = Eigen::Vector4f::Zero();
    spdlog::info("Rendering nerf frame... ");
    testbed.m_fov_axis=1;
    // testbed.m_zoom=1.f;
	  testbed.m_screen_center = Eigen::Vector2f::Constant(0.5f);
    // testbed.m_scale=1.f;
    testbed.set_fov(70);
    testbed.render_frame(camera_matrix,
      camera_matrix,
      rolling_shutter,
      *cudaRenderBuffer);
    spdlog::info("Finished rendering nerf frame.");

    engine.immediate_submit([&](vk::CommandBuffer cmd) {
      vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
      // transfer texture back to transfer dst
      vk::ImageMemoryBarrier imageBarrier_toTransfer;
      imageBarrier_toTransfer.setOldLayout(vk::ImageLayout::eUndefined);
      imageBarrier_toTransfer.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
      imageBarrier_toTransfer.setImage(engine.m_loadedTextures["nerf"]->image.image);
      imageBarrier_toTransfer.setSubresourceRange(range);
      // equivalent to {}
      imageBarrier_toTransfer.setSrcAccessMask(vk::AccessFlagBits::eNone);
      imageBarrier_toTransfer.setDstAccessMask(
          vk::AccessFlagBits::eTransferWrite);
      cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                          vk::PipelineStageFlagBits::eTransfer, {}, nullptr,
                          nullptr, imageBarrier_toTransfer);

      // blit from raw to render texture
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
                    engine.m_loadedTextures["nerf"]->image.image,
                    vk::ImageLayout::eTransferDstOptimal,
                    blitRegion,
                    vk::Filter::eNearest);
      // now transform image back to shader optimal reading
      vk::ImageMemoryBarrier imageBarrier_toReadable;
      imageBarrier_toReadable.setImage(engine.m_loadedTextures["nerf"]->image.image);
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

    prevCameraMatrix = camera_matrix;
  }


	NerfRenderObject::~NerfRenderObject() {
    spdlog::warn("nerfro destroyed!!!");
  }
}