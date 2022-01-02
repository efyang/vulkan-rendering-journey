#include "pipeline.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>

namespace vkr
{
	Pipeline::Pipeline(
		Device &device,
		const std::string &vertFP,
		const std::string &fragFP,
		const PipelineConfigInfo &configInfo) : device(device)
	{
		createGraphicsPipeline(vertFP, fragFP, configInfo);
	}

	Pipeline::~Pipeline()
	{
		vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
		vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
		vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
	}

	void Pipeline::bind(VkCommandBuffer commandBuffer)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	}

	std::vector<char> Pipeline::readFile(const std::string &fp)
	{
		std::ifstream file{fp, std::ios::ate | std::ios::binary};

		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file: " + fp);
		}

		// get file size and make read buffer
		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	void Pipeline::createGraphicsPipeline(const std::string &vertFP, const std::string &fragFP, const PipelineConfigInfo &configInfo)
	{

		assert(configInfo.pipelineLayout != VK_NULL_HANDLE &&
			   "Cannot create graphics pipeline:: no pipelineLayout provided in configInfo");
		assert(configInfo.renderPass != VK_NULL_HANDLE &&
			   "Cannot create graphics pipeline:: no renderPass provided in configInfo");

		auto vertCode = readFile(vertFP);
		auto fragCode = readFile(fragFP);

		std::cout << "Vertex Shader Code Size: " << vertCode.size() << std::endl;
		std::cout << "Fragment Shader Code Size: " << fragCode.size() << std::endl;

		createShaderModule(vertCode, &vertShaderModule);
		createShaderModule(fragCode, &fragShaderModule);

		VkPipelineShaderStageCreateInfo shaderStages[2];
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages[0].module = vertShaderModule;
		shaderStages[0].pName = "main";
		shaderStages[0].flags = 0;
		shaderStages[0].pNext = nullptr;
		shaderStages[0].pSpecializationInfo = nullptr;

		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = fragShaderModule;
		shaderStages[1].pName = "main";
		shaderStages[1].flags = 0;
		shaderStages[1].pNext = nullptr;
		shaderStages[1].pSpecializationInfo = nullptr;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;

		// can actually use multiple viewports/scissors on some GPUs
		VkPipelineViewportStateCreateInfo viewportInfo{};
		viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &configInfo.viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &configInfo.scissor;

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		// just vertex and fragment shader stages
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
		pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
		pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
		pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
		pipelineInfo.pDynamicState = nullptr;

		pipelineInfo.layout = configInfo.pipelineLayout;
		pipelineInfo.renderPass = configInfo.renderPass;
		pipelineInfo.subpass = configInfo.subpass;

		// can be used later to optimize pipeline creation by creating from existing
		pipelineInfo.basePipelineIndex = -1;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(device.device(),
									  VK_NULL_HANDLE,
									  1,
									  &pipelineInfo,
									  nullptr,
									  &graphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create graphics pipeline");
		}
	}

	void Pipeline::createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data()); // would not be valid with c array

		if (vkCreateShaderModule(device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module");
		}
	}

	PipelineConfigInfo Pipeline::defaultPipelineConfigInfo(uint32_t width, uint32_t height)
	{
		PipelineConfigInfo configInfo{};

		// input assembler stage
		// groups input data into triangle point chunks
		configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// triangle_strip is connected, like a mesh, saves mem
		configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// would allow for splitting up strips if using strips using magic numbers
		configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		// define viewport transformation from NDC to image coordinates
		configInfo.viewport.x = 0.0f;
		configInfo.viewport.y = 0.0f;
		configInfo.viewport.width = static_cast<float>(width);
		configInfo.viewport.height = static_cast<float>(height);
		configInfo.viewport.minDepth = 0.0f;
		configInfo.viewport.maxDepth = 1.0f;

		configInfo.scissor.offset = {0, 0};
		configInfo.scissor.extent = {width, height};

		configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		// usually want this off because stuff outside of 0,1 is probably not useful
		configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
		// TODO: ???
		configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
		// how to draw the triangles
		configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		configInfo.rasterizationInfo.lineWidth = 1.0f;
		// cull front or back faces
		// backface culling can be big perf improvement
		configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
		// order of vertices that is the front (winding order)
		// used to determine which face is currently being looked at
		configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		// can be used to alter depth values by some constant or slope
		configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
		configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f; // Optional
		configInfo.rasterizationInfo.depthBiasClamp = 0.0f;			 // Optional
		configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;	 // Optional

		// MSAA
		// take multiple samples along the edge of geometry to determine how much of pixel is in geometry
		configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
		configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		configInfo.multisampleInfo.minSampleShading = 1.0f;			 // Optional
		configInfo.multisampleInfo.pSampleMask = nullptr;			 // Optional
		configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE; // Optional
		configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;		 // Optional

		// how to do mixing
		configInfo.colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
		configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	// Optional
		configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;				// Optional
		configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	// Optional
		configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;				// Optional

		configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
		configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
		configInfo.colorBlendInfo.attachmentCount = 1;
		configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
		configInfo.colorBlendInfo.blendConstants[0] = 0.0f; // Optional
		configInfo.colorBlendInfo.blendConstants[1] = 0.0f; // Optional
		configInfo.colorBlendInfo.blendConstants[2] = 0.0f; // Optional
		configInfo.colorBlendInfo.blendConstants[3] = 0.0f; // Optional

		// depth buffer setup
		configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
		configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
		configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		configInfo.depthStencilInfo.minDepthBounds = 0.0f; // Optional
		configInfo.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
		configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
		configInfo.depthStencilInfo.front = {}; // Optional
		configInfo.depthStencilInfo.back = {};	// Optional

		return configInfo;
	}
}