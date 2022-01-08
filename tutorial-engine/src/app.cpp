#include "app.hpp"

#include <stdexcept>
#include <iostream>
namespace vkr
{
	App::App()
	{
		loadModels();
		createPipelineLayout();
		createPipeline();
		createCommandBuffers();
	}

	App::~App()
	{
		vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
	}

	void App::run()
	{
		while (!window.shouldClose())
		{
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device.device());
	}

	std::vector<Model::Vertex> sierpinski(std::vector<Model::Vertex> vertices)
	{
		// demonstration, not very efficient (need inplace for that)
		std::vector<Model::Vertex> newVertices;
		// assume vertices is multiple of 3
		for (size_t i = 0; i < vertices.size() / 3; i++)
		{
			auto v0 = vertices[3 * i];
			auto v1 = vertices[3 * i + 1];
			auto v2 = vertices[3 * i + 2];

			auto v01 = (v0 + v1) * 0.5f;
			auto v12 = (v1 + v2) * 0.5f;
			auto v20 = (v2 + v0) * 0.5f;

			newVertices.push_back(v0);
			newVertices.push_back(v01);
			newVertices.push_back(v20);

			newVertices.push_back(v01);
			newVertices.push_back(v1);
			newVertices.push_back(v12);

			newVertices.push_back(v20);
			newVertices.push_back(v12);
			newVertices.push_back(v2);
		}
		return newVertices;
	}

	void App::loadModels()
	{
		std::vector<Model::Vertex> vertices{
			{{-0.8f, -0.8f}, {1.0f, 0.0f, 0.0f}},
			{{0.7f, -1.0f}, {0.0f, 1.0f, 0.0f}},
			{{0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		};

		for (int i = 0; i < 5; i++)
		{
			vertices = sierpinski(vertices);
		}

		model = std::make_unique<Model>(device, vertices);
	}

	void App::createPipelineLayout()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		// push constants for small info
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void App::createPipeline()
	{
		// swapchain width and height can be different than windows
		auto pipelineConfig = Pipeline::defaultPipelineConfigInfo(swapchain.width(), swapchain.height());
		// TODO: pull this out on tutorial 8
		pipelineConfig.renderPass = swapchain.getRenderPass();
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/simple_shader.vert.spv",
			"shaders/simple_shader.frag.spv",
			pipelineConfig);
	}

	void App::createCommandBuffers()
	{
		// 1:1 cbuf : framebuffer
		commandBuffers.resize(swapchain.imageCount());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = device.getCommandPool();
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}

		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to begin recording command buffer!");
			}

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = swapchain.getRenderPass();
			renderPassInfo.framebuffer = swapchain.getFrameBuffer(i);

			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = swapchain.getSwapChainExtent();

			std::array<VkClearValue, 2> clearValues{};
			clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
			clearValues[1].depthStencil = {1.0f, 0};
			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();

			// only inline or secondary, not both
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			pipeline->bind(commandBuffers[i]);
			model->bind(commandBuffers[i]);
			model->draw(commandBuffers[i]);

			vkCmdEndRenderPass(commandBuffers[i]);
			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to record command buffer!");
			}
		}
	}

	void App::drawFrame()
	{
		uint32_t imageIndex;
		auto result = swapchain.acquireNextImage(&imageIndex);

		// TODO: handle resizing
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		result = swapchain.submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit command buffers!");
		}
	}
}
