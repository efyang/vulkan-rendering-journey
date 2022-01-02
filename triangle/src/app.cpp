#include "app.hpp"

namespace vkr
{
	void App::run()
	{
		while (!window.shouldClose())
		{
			glfwPollEvents();
		}
	}
}