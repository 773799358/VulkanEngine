#include "vulkanScene.hpp"
#include <include/macro.hpp>

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
		uniformBufferDynamicObjects.resize(meshes.size());
		for (size_t i = 0; i < meshes.size(); i++)
		{
			uniformBufferDynamicObjects[i].model = meshes[i]->node->transform;
			createVertexData(meshes[i]);
			createIndexData(meshes[i]);
		}
		createUniformBufferData();
		createDescriptorSet(); 
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

		for (size_t i = 0; i < nodes.size(); i++)
		{
			delete nodes[i];
		}

		vkDestroyBuffer(device, uniformResource.buffer, nullptr);
		vkFreeMemory(device, uniformResource.memory, nullptr);
		vkDestroyBuffer(device, uniformDynamicResource.buffer, nullptr);
		vkFreeMemory(device, uniformDynamicResource.memory, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptor.layout, nullptr);
	}

	void VulkanRenderSceneData::updateUniformRenderData()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

		UniformBufferObject ubo;
		ubo.proj = cameraController.camera.getProjectMatrix(vulkanRenderer->windowWidth / (float)(vulkanRenderer->windowHeight));
		ubo.view = cameraController.camera.getViewMatrix();

		std::vector<UniformBufferDynamicObject> transforms;
		transforms.resize(meshes.size());
		for (int i = 0; i < meshes.size(); i++)
		{
			transforms[i].model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f / 5.0f), glm::vec3(0.0f, (float)(i * 2 - 1), 0.0f));
			transforms[i].model = glm::translate(glm::mat4(1.0f), glm::vec3((float)(i * 2 - 1), 0.0f, 0.0f)) * transforms[i].model;
		}

		{
			void* data;
			vkMapMemory(vulkanRenderer->device, uniformResource.memory, 0, sizeof(ubo), 0, &data);
			memcpy(data, &ubo, sizeof(ubo));
			vkUnmapMemory(vulkanRenderer->device, uniformResource.memory);
		}

		{
			void* data;
			vkMapMemory(vulkanRenderer->device, uniformDynamicResource.memory, 0, sizeof(transforms), 0, &data);
			memcpy(data, transforms.data(), sizeof(UniformBufferDynamicObject) * transforms.size());
			vkUnmapMemory(vulkanRenderer->device, uniformDynamicResource.memory);
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

	void VulkanRenderSceneData::createUniformBufferData()
	{
		uint32_t uniformBufferSize = sizeof(UniformBufferObject); 
		if (uniformBufferSize > 0)
		{
			vulkanRenderer->createBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformResource.buffer, uniformResource.memory);

			void* data;
			vkMapMemory(vulkanRenderer->device, uniformResource.memory, 0, uniformBufferSize, 0, &data);
			memcpy(data, &uniformBufferObject, uniformBufferSize);
			vkUnmapMemory(vulkanRenderer->device, uniformResource.memory);
		}

		uint32_t uniformDynamicBufferSize = sizeof(UniformBufferDynamicObject) * uniformBufferDynamicObjects.size();
		if (uniformDynamicBufferSize > 0)
		{
			vulkanRenderer->createBuffer(uniformDynamicBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformDynamicResource.buffer, uniformDynamicResource.memory);
		
			void* data;
			vkMapMemory(vulkanRenderer->device, uniformDynamicResource.memory, 0, uniformDynamicBufferSize, 0, &data);
			memcpy(data, uniformBufferDynamicObjects.data(), uniformDynamicBufferSize);
			vkUnmapMemory(vulkanRenderer->device, uniformDynamicResource.memory);
		}
	}

	void VulkanRenderSceneData::createDescriptorSet()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding[2] = {};
		uboLayoutBinding[0].binding = 0;
		uboLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding[0].descriptorCount = 1;
		uboLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;		// 仅在顶点着色器访问
		uboLayoutBinding[0].pImmutableSamplers = nullptr;

		uboLayoutBinding[1].binding = 1;
		uboLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		uboLayoutBinding[1].descriptorCount = 1;
		uboLayoutBinding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding[1].pImmutableSamplers = nullptr;

		//uboLayoutBinding[2].binding = 2;
		//uboLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		//uboLayoutBinding[2].descriptorCount = 1;
		//uboLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;		// 仅在片段着色器访问
		//uboLayoutBinding[2].pImmutableSamplers = nullptr

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = sizeof(uboLayoutBinding) / sizeof(uboLayoutBinding[0]);
		layoutInfo.pBindings = uboLayoutBinding;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRenderer->device, &layoutInfo, nullptr, &descriptor.layout));

		descriptor.descriptorSet.resize(vulkanRenderer->MAX_FRAMES_IN_FLIGHT);
		for (size_t i = 0; i < descriptor.descriptorSet.size(); i++)
		{
			VkDescriptorSetAllocateInfo allocateInfo = {};
			allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocateInfo.pNext = nullptr;
			allocateInfo.descriptorPool = vulkanRenderer->descriptorPool;
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &descriptor.layout;

			VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRenderer->device, &allocateInfo, &descriptor.descriptorSet[i]));
		}

		// TODO:这里什么情况下需要每帧更新呢？
		for (size_t i = 0; i < descriptor.descriptorSet.size(); i++)
		{
			VkDescriptorBufferInfo bufferInfo[2] = {};
			bufferInfo[0].buffer = uniformResource.buffer;
			bufferInfo[0].offset = 0;
			bufferInfo[0].range = sizeof(UniformBufferObject);

			bufferInfo[1].buffer = uniformDynamicResource.buffer;
			bufferInfo[1].offset = 0;
			bufferInfo[1].range = sizeof(UniformBufferDynamicObject);

			//VkDescriptorImageInfo imageInfo = {};
			//imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			//imageInfo.imageView = textureImageView;
			//imageInfo.sampler = textureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptor.descriptorSet[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo[0];
			descriptorWrites[0].pImageInfo = nullptr;
			descriptorWrites[0].pTexelBufferView = nullptr;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptor.descriptorSet[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pBufferInfo = &bufferInfo[1];
			descriptorWrites[1].pImageInfo = nullptr;
			descriptorWrites[1].pTexelBufferView = nullptr;

			//descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			//descriptorWrites[2].dstSet = descriptorSets[i];
			//descriptorWrites[2].dstBinding = 2;
			//descriptorWrites[2].dstArrayElement = 0;
			//descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			//descriptorWrites[2].descriptorCount = 1;
			//descriptorWrites[2].pBufferInfo = nullptr;
			//descriptorWrites[2].pImageInfo = &imageInfo;
			//descriptorWrites[2].pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(vulkanRenderer->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	StaticMesh* VulkanRenderSceneData::createCube()
	{
		StaticMesh* mesh = new StaticMesh();
		Node* node = new Node();
		node->transform = glm::mat4(1.0f);
		mesh->node = node;
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

		return mesh;
	}

}