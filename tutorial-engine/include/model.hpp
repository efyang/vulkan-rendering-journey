#pragma once

#include "device.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

#include <vector>

namespace vkr
{
	// short term model, will run into allocator issues on complex scenes
	// eventually integrate VMA
	class Model
	{
	public:
		struct Vertex
		{
			glm::vec2 position;
			glm::vec3 color;

			Vertex(glm::vec2 position, glm::vec3 color) : position{position}, color{color} {}

			Vertex operator+(const Vertex &rhs)
			{
				return Vertex(
					this->position + rhs.position,
					this->color + rhs.color);
			}

			Vertex operator*(float rhs)
			{
				return Vertex(this->position *= rhs,
							  this->color *= rhs);
			}

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
		};

		Model(Device &device, const std::vector<Vertex> &vertices);
		~Model();

		Model(const Model &) = delete;			  // remove copy constructors
		Model &operator=(const Model &) = delete; // remove copy constructors

		void bind(VkCommandBuffer commandBuffer);
		void draw(VkCommandBuffer commandBuffer);

	private:
		void createVertexBuffers(const std::vector<Vertex> &vertices);
		Device &device;
		VkBuffer vertexBuffer;
		VkDeviceMemory vertexBufferMemory;
		// TODO: larger sized?
		uint32_t vertexCount;
	};
}