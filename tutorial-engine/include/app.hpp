#pragma once

#include "pipeline.hpp"
#include "window.hpp"
#include "swapchain.hpp"
#include "model.hpp"

#include <memory>
#include <vector>

namespace vkr
{
	class App
	{
	public:
		static constexpr int WIDTH = 1900;
		static constexpr int HEIGHT = 1000;

		App();
		~App();
		void run();

		App(const App &) = delete;			  // remove copy constructors
		App &operator=(const App &) = delete; // remove copy constructors

	private:
		void loadModels();
		Window window{WIDTH, HEIGHT, "Hello Vulkan!"};
		Device device{window};
		std::unique_ptr<Pipeline> pipeline;
		std::unique_ptr<SwapChain> swapchain;

		VkPipelineLayout pipelineLayout;
		std::vector<VkCommandBuffer> commandBuffers;
		std::unique_ptr<Model> model;

		void createPipelineLayout();
		void createPipeline();
		void createCommandBuffers();
		void freeCommandBuffers();
		void drawFrame();
		void recreateSwapChain();
		void recordCommandBuffer(int imageIndex);
	};
}
