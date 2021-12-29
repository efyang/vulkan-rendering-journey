#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
namespace vkr
{
	class VkrWindow
	{
	public:
		VkrWindow(int w, int h, std::string name);
		bool shouldClose();
		~VkrWindow();

	private:
		void init();
		const int width;
		const int height;

		std::string windowName;
		GLFWwindow *window;
	};
}