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
		void operator=(const App &) = delete; // remove copy constructors

	private:
		void loadModels();
		Window window{WIDTH, HEIGHT, "Hello Vulkan!"};
		Device device{window};
		SwapChain swapchain{device, window.getExtent()};
		std::unique_ptr<Pipeline> pipeline;

		VkPipelineLayout pipelineLayout;
		std::vector<VkCommandBuffer> commandBuffers;
		std::unique_ptr<Model> model;

		void createPipelineLayout();
		void createPipeline();
		void createCommandBuffers();
		void drawFrame();
	};
}
