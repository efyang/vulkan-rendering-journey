#include "window.hpp"
#include <stdexcept>

namespace vkr
{
	Window::Window(int w, int h, std::string name) : width{w}, height{h}, windowName{name}
	{
		init();
	}

	Window::~Window()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	bool Window::shouldClose()
	{
		return glfwWindowShouldClose(window);
	}

	void Window::init()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface");
		}
	}

	void Window::framebufferResizeCallback(GLFWwindow *window, int width, int height)
	{
		auto userptr_window = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
		userptr_window->frameBufferResized = true;
		userptr_window->width = width;
		userptr_window->height = height;
	}
}
