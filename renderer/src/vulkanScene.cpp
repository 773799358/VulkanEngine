#include "vulkanScene.hpp"

namespace VulkanEngine
{
	VkVertexInputBindingDescription Vertex::getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		// 暂不使用instance

		return bindingDescription;
	}

	std::array<VkVertexInputAttributeDescription, 4> Vertex::getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions = {};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, normal);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, texcoord);

		return attributeDescriptions;
	}

	bool Vertex::operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && texcoord == other.texcoord && normal == other.normal;
	}

	void VulkanRenderSceneData::init(VulkanRenderer* vulkanRenderer)
	{
		this->vulkanRenderer = vulkanRenderer;
	}

	void VulkanRenderSceneData::setupRenderData()
	{
	}

	void VulkanRenderSceneData::clear()
	{
		auto& device = vulkanRenderer->device;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			vkDestroyBuffer(device, meshes[i]->vertexBuffer.buffer, nullptr);
			vkFreeMemory(device, meshes[i]->vertexBuffer.memory, nullptr);
			vkDestroyBuffer(device, meshes[i]->indexBuffer.buffer, nullptr);
			vkFreeMemory(device, meshes[i]->indexBuffer.memory, nullptr);
			delete meshes[i];
		}

	}

	VkSampler VulkanRenderSceneData::getLinearSampler()
	{
		if (linearSampler == VK_NULL_HANDLE)
		{
			
		}

		return linearSampler;
	}

	void VulkanRenderSceneData::createVertexData(StaticMesh* mesh)
	{
		auto& vertices = mesh->vertices;
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(vulkanRenderer->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(vulkanRenderer->device, stagingBufferMemory);

		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh->vertexBuffer.buffer, mesh->vertexBuffer.memory);

		vulkanRenderer->copyBuffer(stagingBuffer, mesh->vertexBuffer.buffer, bufferSize);

		vkDestroyBuffer(vulkanRenderer->device, stagingBuffer, nullptr);
		vkFreeMemory(vulkanRenderer->device, stagingBufferMemory, nullptr);
	}

	void VulkanRenderSceneData::createIndexData(StaticMesh* mesh)
	{
		auto& indices = mesh->indices;
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(vulkanRenderer->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(vulkanRenderer->device, stagingBufferMemory);

		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh->indexBuffer.buffer, mesh->indexBuffer.memory);

		vulkanRenderer->copyBuffer(stagingBuffer, mesh->indexBuffer.buffer, bufferSize);

		vkDestroyBuffer(vulkanRenderer->device, stagingBuffer, nullptr);
		vkFreeMemory(vulkanRenderer->device, stagingBufferMemory, nullptr);
	}

	StaticMesh* VulkanRenderSceneData::createCube()
	{
		StaticMesh* mesh = new StaticMesh();
		mesh->vertices =
		{
			// positions             // colors          // normals          // texture coords
			{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{0.0f,  0.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{1.0f,  0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{1.0f,  1.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{1.0f,  1.0f}},
			{{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{0.0f,  1.0f}},
			{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{0.0f,  0.0f}},

			{{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{0.0f,  0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{1.0f,  0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{1.0f,  1.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{1.0f,  1.0f}},
			{{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{0.0f,  1.0f}},
			{{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{0.0f,  0.0f}},

			{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{1.0f,  0.0f}},
			{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{1.0f,  1.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{0.0f,  1.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{0.0f,  1.0f}},
			{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{0.0f,  0.0f}},
			{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{1.0f,  0.0f}},

			{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{1.0f,  0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{1.0f,  1.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{0.0f,  1.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{0.0f,  1.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{0.0f,  0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{1.0f,  0.0f}},

			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{0.0f,  1.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{1.0f,  1.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{1.0f,  0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{1.0f,  0.0f}},
			{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{0.0f,  0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{0.0f,  1.0f}},

			{{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{0.0f,  1.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{1.0f,  1.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{1.0f,  0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{1.0f,  0.0f}},
			{{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{0.0f,  0.0f}},
			{{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{0.0f,  1.0f}}
		};

		mesh->indices =
		{
		   2, 1, 0, 5, 4, 3,
		   6, 7, 8, 9, 10,11,
		   12,13,14,15,16,17 ,
		   20,19,18,23,22,21,
		   24,25,26,27,28,29 ,
		   32,31,30,35,34,33
		};

		createVertexData(mesh);
		createIndexData(mesh);

		return mesh;
	}

}