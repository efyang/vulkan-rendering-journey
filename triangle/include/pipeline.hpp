#pragma once

#include <string>
#include <vector>

namespace vkr
{
	class Pipeline
	{
	public:
		Pipeline(const std::string &vertFP, const std::string &fragFP);

	private:
		static std::vector<char> readFile(const std::string &fp);
		void createGraphicsPipeline(const std::string &vertFP, const std::string &fragFP);
	};
}