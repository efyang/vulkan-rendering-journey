#include "app.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
using namespace std;

int main()
{

	vkr::VkrApp app{};
	try {
		app.run();
	} catch (const std::exception &e) {
		std::cerr << e.what() << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
