#include "pipeline.hpp"

#include <fstream>
#include <vector>
#include <string>
#include <iostream>

namespace vkr
{
	Pipeline::Pipeline(const std::string &vertFP, const std::string &fragFP)
	{
		createGraphicsPipeline(vertFP, fragFP);
	}

	std::vector<char> Pipeline::readFile(const std::string &fp)
	{
		std::ifstream file{fp, std::ios::ate | std::ios::binary};

		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file: " + fp);
		}

		// get file size and make read buffer
		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	void Pipeline::createGraphicsPipeline(const std::string &vertFP, const std::string &fragFP)
	{
		auto vertCode = readFile(vertFP);
		auto fragCode = readFile(fragFP);

		std::cout << "Vertex Shader Code Size: " << vertCode.size() << std::endl;
		std::cout << "Fragment Shader Code Size: " << fragCode.size() << std::endl;
	}
}