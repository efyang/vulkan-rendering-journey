#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
namespace vkr
{
	class Window
	{
	public:
		Window(int w, int h, std::string name);
		bool shouldClose();
		void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
		~Window();

	private:
		void init();
		const int width;
		const int height;

		std::string windowName;
		GLFWwindow *window;
	};
}