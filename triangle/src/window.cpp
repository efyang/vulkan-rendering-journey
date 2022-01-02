#include "window.hpp"

namespace vkr {
	VkrWindow::VkrWindow(int w, int h, std::string name) : width{w}, height{h}, windowName{name} {
		init();
	}

	VkrWindow::~VkrWindow() {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	bool VkrWindow::shouldClose() {
		return glfwWindowShouldClose(window);
	}

	void VkrWindow::init() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
	}
}
