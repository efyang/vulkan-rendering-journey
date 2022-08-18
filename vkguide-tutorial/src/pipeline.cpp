#include "pipeline.hpp"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

namespace vkr {

vk::Pipeline PipelineBuilder::build(vk::Device device, vk::RenderPass pass) {
  vk::PipelineViewportStateCreateInfo viewportState;
  viewportState.setViewports(viewport);
  viewportState.setScissors(scissor);

  // dummy color blending
  vk::PipelineColorBlendStateCreateInfo colorBlending;
  colorBlending.setLogicOpEnable(false);
  colorBlending.setAttachments(colorBlendAttachment);

  vk::GraphicsPipelineCreateInfo pipelineInfo;
  pipelineInfo.setStages(shaderStages);
  pipelineInfo.setPVertexInputState(&vertexInputInfo);
  pipelineInfo.setPInputAssemblyState(&inputAssembly);
  pipelineInfo.setPViewportState(&viewportState);
  pipelineInfo.setPRasterizationState(&rasterizer);
  pipelineInfo.setPMultisampleState(&multisampling);
  pipelineInfo.setPColorBlendState(&colorBlending);
  pipelineInfo.setLayout(pipelineLayout);
  pipelineInfo.renderPass = pass;
  pipelineInfo.subpass = 0;
  pipelineInfo.setBasePipelineHandle(VK_NULL_HANDLE);
  pipelineInfo.setPDepthStencilState(&depthStencil);

  vk::Pipeline pipeline =
      device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo).value;
  spdlog::info("Successfully created pipeline");
  return pipeline;
}

vk::PipelineShaderStageCreateInfo
PipelineBuilder::default_pipeline_shader_stage_create_info(
    vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule) {
  vk::PipelineShaderStageCreateInfo info;
  info.setStage(stage);
  info.setModule(shaderModule);
  info.setPName("main");
  return info;
}

vk::PipelineVertexInputStateCreateInfo
PipelineBuilder::default_vertex_input_state_create_info() {
  vk::PipelineVertexInputStateCreateInfo info;
  info.vertexBindingDescriptionCount = 0;
  info.vertexAttributeDescriptionCount = 0;
  return info;
}

vk::PipelineInputAssemblyStateCreateInfo
PipelineBuilder::default_pipeline_input_assembly_create_info(
    vk::PrimitiveTopology topology) {
  vk::PipelineInputAssemblyStateCreateInfo info;
  info.setTopology(topology);
  info.setPrimitiveRestartEnable(false);
  return info;
}

vk::PipelineRasterizationStateCreateInfo
PipelineBuilder::default_rasterization_state_create_info(
    vk::PolygonMode polygonMode) {
  vk::PipelineRasterizationStateCreateInfo info;
  info.setDepthClampEnable(false);
  info.setRasterizerDiscardEnable(false);
  info.setPolygonMode(polygonMode);
  info.setLineWidth(1.0);
  info.setCullMode(vk::CullModeFlagBits::eNone);
  info.setFrontFace(vk::FrontFace::eClockwise);
  info.setDepthBiasEnable(false);
  return info;
}

vk::PipelineMultisampleStateCreateInfo
PipelineBuilder::default_multisampling_state_create_info() {
  vk::PipelineMultisampleStateCreateInfo info;
  info.setSampleShadingEnable(false);
  info.setRasterizationSamples(vk::SampleCountFlagBits::e1);
  info.setMinSampleShading(1.0);
  info.setPSampleMask(nullptr);
  info.setAlphaToCoverageEnable(false);
  info.setAlphaToOneEnable(false);
  return info;
}

vk::PipelineColorBlendAttachmentState
PipelineBuilder::default_color_blend_attachment_state() {
  vk::PipelineColorBlendAttachmentState attachment;
  attachment.setColorWriteMask(
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
  attachment.setBlendEnable(false);
  return attachment;
}

vk::PipelineLayoutCreateInfo
PipelineBuilder::default_pipeline_layout_create_info() {
  vk::PipelineLayoutCreateInfo info;
  info.setFlags({});
  info.setSetLayouts(nullptr);
  info.setPushConstantRanges(nullptr);

  return info;
}

vk::PipelineDepthStencilStateCreateInfo
PipelineBuilder::default_depth_stencil_create_info(bool bDepthTest,
                                                   bool bDepthWrite,
                                                   vk::CompareOp compareOp) {
  vk::PipelineDepthStencilStateCreateInfo info;
  info.setDepthTestEnable(bDepthTest)
      .setDepthWriteEnable(bDepthWrite)
      .setDepthCompareOp(bDepthTest ? compareOp : vk::CompareOp::eAlways)
      .setDepthBoundsTestEnable(false)
      .setMinDepthBounds(0.f)
      .setMaxDepthBounds(1.f)
      .setStencilTestEnable(false);
  return info;
}

} // namespace vkr