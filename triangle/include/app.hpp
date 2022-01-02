#pragma once

#include "window.hpp"
#include "pipeline.hpp"

namespace vkr {
	class VkrApp {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;
		void run();

	private:
		VkrWindow vkrWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
		Pipeline pipeline{
			"shaders/simple_shader.vert.spv",
			"shaders/simple_shader.frag.spv"};
	};
}
