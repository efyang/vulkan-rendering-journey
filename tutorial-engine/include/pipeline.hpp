#pragma once

#include <string>
#include <vector>

#include "device.hpp"

namespace vkr
{
	struct PipelineConfigInfo
	{
		VkViewport viewport;
		VkRect2D scissor;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
		VkPipelineRasterizationStateCreateInfo rasterizationInfo;
		VkPipelineMultisampleStateCreateInfo multisampleInfo;
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		VkPipelineColorBlendStateCreateInfo colorBlendInfo;
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
		// not set here, must be set by user
		VkPipelineLayout pipelineLayout = nullptr;
		VkRenderPass renderPass = nullptr;
		uint32_t subpass = 0;
	};

	class Pipeline
	{
	public:
		Pipeline(
			Device &device,
			const std::string &vertFP,
			const std::string &fragFP,
			const PipelineConfigInfo &configInfo);
		~Pipeline(); // pipeline handles resource lifetimes

		Pipeline(const Pipeline &) = delete;			// remove copy constructors
		Pipeline &operator=(const Pipeline &) = delete; // remove copy constructors

		void bind(VkCommandBuffer commandBuffer);
		static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);

	private:
		static std::vector<char> readFile(const std::string &fp);
		void createGraphicsPipeline(const std::string &vertFP,
									const std::string &fragFP,
									const PipelineConfigInfo &configInfo);

		void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);
		Device &device; // will always outlive pipeline
		VkPipeline graphicsPipeline;
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;
	};
} // namespace vkr