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
		~Window();

		Window(const Window &) = delete;
		Window &operator=(const Window &) = delete;

		bool shouldClose();
		VkExtent2D getExtent()
		{
			return {static_cast<uint32_t>(width),
					static_cast<uint32_t>(height)};
		}
		bool wasWindowResize() { return frameBufferResized; }
		void resetWindowResizedFlag() { frameBufferResized = false; }

		void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

	private:
		static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
		void init();
		int width;
		int height;
		bool frameBufferResized = false;

		std::string windowName;
		GLFWwindow *window;
	};
}