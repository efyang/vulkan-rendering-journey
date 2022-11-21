#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vkr {
class PipelineBuilder {
public:
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
  vk::Viewport viewport;
  vk::Rect2D scissor;
  vk::PipelineRasterizationStateCreateInfo rasterizer;
  vk::PipelineColorBlendAttachmentState colorBlendAttachment;
  vk::PipelineMultisampleStateCreateInfo multisampling;
  vk::PipelineDepthStencilStateCreateInfo depthStencil;
  vk::PipelineLayout pipelineLayout;

  vk::Pipeline build(vk::Device device, vk::RenderPass pass);

  static vk::PipelineShaderStageCreateInfo
  default_pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage,
                                            vk::ShaderModule shaderModule);
  static vk::PipelineVertexInputStateCreateInfo
  default_vertex_input_state_create_info();

  static vk::PipelineInputAssemblyStateCreateInfo
  default_pipeline_input_assembly_create_info(vk::PrimitiveTopology topology);

  static vk::PipelineRasterizationStateCreateInfo
  default_rasterization_state_create_info(vk::PolygonMode polygonMode);

  static vk::PipelineMultisampleStateCreateInfo
  default_multisampling_state_create_info();

  static vk::PipelineColorBlendAttachmentState
  default_color_blend_attachment_state();

  static vk::PipelineLayoutCreateInfo default_pipeline_layout_create_info();

  static vk::PipelineDepthStencilStateCreateInfo
  default_depth_stencil_create_info(bool bDepthTest, bool bDepthWrite,
                                    vk::CompareOp compareOp);

private:
};
}; // namespace vkr