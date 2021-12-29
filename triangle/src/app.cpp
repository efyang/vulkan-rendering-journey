#include "app.hpp"

namespace vkr
{
	void VkrApp::run()
	{
		while (!vkrWindow.shouldClose())
		{
			glfwPollEvents();
		}
	}
}