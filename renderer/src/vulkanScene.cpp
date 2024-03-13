#include "vulkanScene.hpp"
#include <include/macro.hpp>
#include "vulkanUtil.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace VulkanEngine
{
	std::vector<VkVertexInputBindingDescription> Vertex::getBindingDescriptions()
	{
		std::vector<VkVertexInputBindingDescription> bindingDescriptions = { {} };
		
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(Vertex);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		// 暂不使用instance

		return bindingDescriptions;
	}

	std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions = { {}, {}, {}, {}, {} };

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

		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(Vertex, tangent);

		return attributeDescriptions;
	}

	void VulkanRenderSceneData::init(VulkanRenderer* vulkanRenderer)
	{
		this->vulkanRenderer = vulkanRenderer;

		// TODO:没设置默认AO和自发光贴图
		PBRMaterial* material = new PBRMaterial();

		std::string defaultPaht = vulkanRenderer->basePath + "/resources/models/default/";

		Texture* diffuse = new Texture();
		diffuse->fullPath = defaultPaht + "/default_grey.png";
		Texture* normal = new Texture();
		normal->fullPath = defaultPaht + "/default_normal.png";
		Texture* metallicRoughness = new Texture();
		metallicRoughness->fullPath = defaultPaht + "/default_mr.jpg";

		textures.resize(3);
		textures[0] = diffuse;
		textures[1] = normal;
		textures[2] = metallicRoughness;

		material->baseColor = diffuse;
		material->normal = normal;
		material->metallicRoughness = metallicRoughness;

		materials.push_back(material);
	}

	void VulkanRenderSceneData::setupRenderData()
	{
		// 配置一次subpass的状态，DX内对应PipelineStateObject
		std::string shaderDir = vulkanRenderer->basePath + "spvs/";

		shaderVSFliePath = shaderDir + "vs" + ".vert.spv";
		shaderFSFilePath = shaderDir + shaderName + ".frag.spv";

		GBufferFSFilePath = shaderDir + "gbuffer" + ".frag.spv";

		shadowVSFilePath = shaderDir + "directionalLightShadow" + ".vert.spv";
		shadowFSFilePath = shaderDir + "directionalLightShadow" + ".frag.spv";

		uniformBufferDynamicObjects.resize(meshes.size());
		for (size_t i = 0; i < meshes.size(); i++)
		{
			uniformBufferDynamicObjects[i].model = meshes[i]->node->worldTransform;
		}
		createVertexData();
		createIndexData();
		createUniformBufferData();
		createUniformDescriptorSet();
		createPBRDescriptorLayout();

		for (size_t i = 0; i < textures.size(); i++)
		{
			textures[i]->createTextureImage(vulkanRenderer);
		}

		for (size_t i = 0; i < materials.size(); i++)
		{
			materials[i]->createDescriptorSet(vulkanRenderer, this);
		}
	}

	void VulkanRenderSceneData::clear()
	{
		auto& device = vulkanRenderer->device;

		vkDestroyBuffer(device, vertexResource.buffer, nullptr);
		vkFreeMemory(device, vertexResource.memory, nullptr);
		vkDestroyBuffer(device, indexResource.buffer, nullptr);
		vkFreeMemory(device, indexResource.memory, nullptr);

		for (size_t i = 0; i < meshes.size(); i++)
		{
			delete meshes[i];
		}

		for (size_t i = 0; i < textures.size(); i++)
		{
			vkDestroyImage(device, textures[i]->textureImage, nullptr);
			vkDestroyImageView(device, textures[i]->textureImageView, nullptr);
			vkFreeMemory(device, textures[i]->textureImageMemory, nullptr);
			delete textures[i];
		}

		for (size_t i = 0; i < materials.size(); i++)
		{
			vkFreeDescriptorSets(device, vulkanRenderer->descriptorPool, 1, &materials[i]->descriptorSet);
			delete materials[i];
		}

		for (size_t i = 0; i < nodes.size(); i++)
		{
			delete nodes[i];
		}

		vkDestroyBuffer(device, uniformResource.buffer, nullptr);
		vkFreeMemory(device, uniformResource.memory, nullptr);
		vkDestroyBuffer(device, uniformDynamicResource.buffer, nullptr);
		vkFreeMemory(device, uniformDynamicResource.memory, nullptr);
		vkDestroyBuffer(device, uniformShadowResource.buffer, nullptr);
		vkFreeMemory(device, uniformShadowResource.memory, nullptr);

		vkDestroyDescriptorSetLayout(device, uniformDescriptor.layout, nullptr);
		vkFreeDescriptorSets(device, vulkanRenderer->descriptorPool, uniformDescriptor.descriptorSet.size(), uniformDescriptor.descriptorSet.data());
		vkDestroyDescriptorSetLayout(device, PBRMaterialDescriptor.layout, nullptr);
		if (directionalLightShadowDescriptor.layout != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(device, directionalLightShadowDescriptor.layout, nullptr);
			vkFreeDescriptorSets(device, vulkanRenderer->descriptorPool, directionalLightShadowDescriptor.descriptorSet.size(), directionalLightShadowDescriptor.descriptorSet.data());
		}
	}

	void VulkanRenderSceneData::updateUniformRenderData()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

		uniformBufferVSObject.proj = cameraController.camera.getProjectMatrix(vulkanRenderer->windowWidth / (float)(vulkanRenderer->windowHeight));
		uniformBufferVSObject.view = cameraController.camera.getViewMatrix();

		uniformBufferFSObject.viewPos = cameraController.camera.position;
		uniformBufferFSObject.directionalLightPos = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f / 5.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(1.0f, 3.0f, 1.0f, 1.0f);

		float sceneSphereRadius = glm::length(getSceneBounds().getSize()) / 2.0f;
		glm::vec3 sceneSphereCenter = getSceneBounds().getCenter();
		glm::vec3 directionalLightPos = uniformBufferFSObject.directionalLightPos;
		glm::vec3 shadowCameraPos = sceneSphereCenter + glm::normalize(directionalLightPos) * sceneSphereRadius * 4.0f;
		float near = glm::length(shadowCameraPos - sceneSphereCenter) - sceneSphereRadius;
		float far = near + sceneSphereRadius * 2.0f;

		glm::mat4 shadowView = glm::lookAtRH(shadowCameraPos, sceneSphereCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 shadowProj = glm::perspective(glm::radians(45.0f), 1.0f, near, far);
		shadowProj[1][1] *= -1;

		uniformBufferShadowVSObject.projectView = shadowProj * shadowView;

		uniformBufferFSObject.directionalLightProjView = uniformBufferShadowVSObject.projectView;

		std::vector<UniformBufferDynamicObject> transforms;
		transforms.resize(meshes.size());
		for (int i = 0; i < meshes.size(); i++)
		{
			transforms[i].model =  rotate * meshes[i]->node->worldTransform;
			//transforms[i].model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f / 5.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * transforms[i].model;
		}

		{
			void* data;
			vkMapMemory(vulkanRenderer->device, uniformResource.memory, 0, sizeof(uniformBufferVSObject) + sizeof(uniformBufferFSObject), 0, &data);
			memcpy(data, &uniformBufferVSObject, sizeof(uniformBufferVSObject));
			memcpy(((char*)(data) + sizeof(uniformBufferVSObject)), &uniformBufferFSObject, sizeof(uniformBufferFSObject));
			vkUnmapMemory(vulkanRenderer->device, uniformResource.memory);
		}

		{
			void* data;
			vkMapMemory(vulkanRenderer->device, uniformDynamicResource.memory, 0, sizeof(transforms), 0, &data);
			memcpy(data, transforms.data(), sizeof(UniformBufferDynamicObject) * transforms.size());
			vkUnmapMemory(vulkanRenderer->device, uniformDynamicResource.memory);
		}

		{
			void* data;
			vkMapMemory(vulkanRenderer->device, uniformShadowResource.memory, 0, sizeof(uniformBufferShadowVSObject), 0, &data);
			memcpy(data, &uniformBufferShadowVSObject, sizeof(uniformBufferShadowVSObject));
			vkUnmapMemory(vulkanRenderer->device, uniformShadowResource.memory);
		}
	}

	Box VulkanRenderSceneData::getSceneBounds()
	{
		Box box;

		for (size_t i = 0; i < meshes.size(); i++)
		{
			Mesh* mesh = meshes[i];
			for (size_t vertexIndex = 0; vertexIndex < mesh->vertices.size(); vertexIndex++)
			{
				box.addPoint(mesh->node->worldTransform * glm::vec4(mesh->vertices[vertexIndex].position, 1.0f));
			}
		}

		return box;
	}

	void VulkanRenderSceneData::lookAtSceneCenter()
	{
		Box box = getSceneBounds();
		// 这里注意，直接调用vec3.length，是分量个数
		float radius = glm::length(box.getSize());
		radius *= 2.0f * glm::sqrt(2.0f);
		cameraController.setCenterAndRadius(box.getCenter(), radius);
	}

	void VulkanRenderSceneData::createVertexData()
	{
		VkDeviceSize bufferSize = 0;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			bufferSize += meshes[i]->vertices.size() * sizeof(Vertex);
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(vulkanRenderer->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		uint32_t offset = 0;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			memcpy((char*)(data) + offset, meshes[i]->vertices.data(), sizeof(Vertex) * meshes[i]->vertices.size());
			offset += sizeof(Vertex) * meshes[i]->vertices.size();
		}
		vkUnmapMemory(vulkanRenderer->device, stagingBufferMemory);

		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexResource.buffer, vertexResource.memory);

		vulkanRenderer->copyBuffer(stagingBuffer, vertexResource.buffer, bufferSize);

		vkDestroyBuffer(vulkanRenderer->device, stagingBuffer, nullptr);
		vkFreeMemory(vulkanRenderer->device, stagingBufferMemory, nullptr);
	}

	void VulkanRenderSceneData::createIndexData()
	{
		VkDeviceSize bufferSize = 0;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			bufferSize += meshes[i]->indices.size() * sizeof(uint32_t);
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(vulkanRenderer->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		uint32_t offset = 0;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			memcpy((char*)(data) + offset, meshes[i]->indices.data(), sizeof(uint32_t) * meshes[i]->indices.size());
			offset += sizeof(uint32_t) * meshes[i]->indices.size();
		}
		vkUnmapMemory(vulkanRenderer->device, stagingBufferMemory);

		vulkanRenderer->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexResource.buffer, indexResource.memory);

		vulkanRenderer->copyBuffer(stagingBuffer, indexResource.buffer, bufferSize);

		vkDestroyBuffer(vulkanRenderer->device, stagingBuffer, nullptr);
		vkFreeMemory(vulkanRenderer->device, stagingBufferMemory, nullptr);
	}

	void VulkanRenderSceneData::createUniformBufferData()
	{
		uint32_t uniformBufferSize = sizeof(UniformBufferObjectVS) + sizeof(UniformBufferObjectFS); 
		if (uniformBufferSize > 0)
		{
			vulkanRenderer->createBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformResource.buffer, uniformResource.memory);
		}

		uint32_t uniformDynamicBufferSize = sizeof(UniformBufferDynamicObject) * uniformBufferDynamicObjects.size();
		if (uniformDynamicBufferSize > 0)
		{
			vulkanRenderer->createBuffer(uniformDynamicBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformDynamicResource.buffer, uniformDynamicResource.memory);
		}

		uint32_t uniformBufferShadowSize = sizeof(uniformBufferShadowVSObject);
		if (uniformBufferShadowSize > 0)
		{
			vulkanRenderer->createBuffer(uniformBufferShadowSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformShadowResource.buffer, uniformShadowResource.memory);
		}
	}

	void VulkanRenderSceneData::createPBRDescriptorLayout()
	{
		// diffuse
		VkDescriptorSetLayoutBinding PBRLayoutBinding[3] = {};
		PBRLayoutBinding[0].binding = 0;
		PBRLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		PBRLayoutBinding[0].descriptorCount = 1;
		PBRLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		PBRLayoutBinding[0].pImmutableSamplers = nullptr;
		// normal
		PBRLayoutBinding[1].binding = 1;
		PBRLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		PBRLayoutBinding[1].descriptorCount = 1;
		PBRLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		PBRLayoutBinding[1].pImmutableSamplers = nullptr;
		// mr
		PBRLayoutBinding[2].binding = 2;
		PBRLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; 
		PBRLayoutBinding[2].descriptorCount = 1;
		PBRLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		PBRLayoutBinding[2].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = sizeof(PBRLayoutBinding) / sizeof(PBRLayoutBinding[0]);
		layoutInfo.pBindings = PBRLayoutBinding;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRenderer->device, &layoutInfo, nullptr, &PBRMaterialDescriptor.layout));
	}

	void VulkanRenderSceneData::createUniformDescriptorSet()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding[3] = {};
		uboLayoutBinding[0].binding = 0;
		uboLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding[0].descriptorCount = 1;
		uboLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;		// 仅在顶点着色器访问
		uboLayoutBinding[0].pImmutableSamplers = nullptr;

		uboLayoutBinding[1].binding = 1;
		uboLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding[1].descriptorCount = 1;
		uboLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		uboLayoutBinding[1].pImmutableSamplers = nullptr;

		uboLayoutBinding[2].binding = 2;
		uboLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		uboLayoutBinding[2].descriptorCount = 1;
		uboLayoutBinding[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding[2].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = sizeof(uboLayoutBinding) / sizeof(uboLayoutBinding[0]);
		layoutInfo.pBindings = uboLayoutBinding;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRenderer->device, &layoutInfo, nullptr, &uniformDescriptor.layout));

		// TODO:这里一个set是不是就够用了，vulkan tutorial用的是frame count，从semaphore的依赖上看起来不会有问题
		uniformDescriptor.descriptorSet.resize(1);
		for (size_t i = 0; i < uniformDescriptor.descriptorSet.size(); i++)
		{
			VkDescriptorSetAllocateInfo allocateInfo = {};
			allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocateInfo.pNext = nullptr;
			allocateInfo.descriptorPool = vulkanRenderer->descriptorPool;
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &uniformDescriptor.layout;

			VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRenderer->device, &allocateInfo, &uniformDescriptor.descriptorSet[i]));
		}

		for (size_t i = 0; i < uniformDescriptor.descriptorSet.size(); i++)
		{
			VkDescriptorBufferInfo bufferInfo[3] = {};
			bufferInfo[0].buffer = uniformResource.buffer;
			bufferInfo[0].offset = 0;
			bufferInfo[0].range = sizeof(UniformBufferObjectVS);

			bufferInfo[1].buffer = uniformResource.buffer;
			bufferInfo[1].offset = sizeof(UniformBufferObjectVS);
			bufferInfo[1].range = sizeof(UniformBufferObjectFS);

			bufferInfo[2].buffer = uniformDynamicResource.buffer;
			bufferInfo[2].offset = 0;
			bufferInfo[2].range = sizeof(UniformBufferDynamicObject);

			std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = uniformDescriptor.descriptorSet[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo[0];
			descriptorWrites[0].pImageInfo = nullptr;
			descriptorWrites[0].pTexelBufferView = nullptr;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = uniformDescriptor.descriptorSet[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pBufferInfo = &bufferInfo[1];
			descriptorWrites[1].pImageInfo = nullptr;
			descriptorWrites[1].pTexelBufferView = nullptr;

			descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[2].dstSet = uniformDescriptor.descriptorSet[i];
			descriptorWrites[2].dstBinding = 2;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].pBufferInfo = &bufferInfo[2];
			descriptorWrites[2].pImageInfo = nullptr;
			descriptorWrites[2].pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(vulkanRenderer->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}
	 
	Box::Box()
	{
		min.x = min.y = min.z = 1e30f;
		max.x = max.y = max.z = -1e30f;
	}

	void Box::addPoint(const glm::vec3& point)
	{
		max = glm::max(point, max);
		min = glm::min(point, min);
	}

	void Box::unionBox(const Box& box)
	{
		if (isValid() && box.isValid())
		{
			min = glm::min(box.min, min);
			max = glm::max(box.max, max);
		}
		else if (box.isValid())
		{
			*this = box;
		}
	}

	glm::vec3 Box::getCenter()
	{
		return (min + max) / 2.0f;
	}

	glm::vec3 Box::getSize()
	{
		return (max - min);
	}

	bool Box::isValid() const
	{
		return max.x >= min.x && max.y >= min.y && max.z >= min.z;
	}

	void Texture::createTextureImage(VulkanRenderer* vulkanRender)
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);		// 强制4通道，有利于对齐

		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		VkDeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels)
		{
			LOG_ERROR("failed to load texture image : {}", fullPath);
		}

		vulkanRender->createTextureImage(textureImage, textureImageView, textureImageMemory, texWidth, texHeight, pixels, mipLevels);

		sampler = vulkanRender->getOrCreateMipmapSampler(mipLevels);

		stbi_image_free(pixels);
	}

	void PBRMaterial::createDescriptorSet(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData)
	{
		VkDescriptorSetAllocateInfo PBRMaterialDescriptorSetAllocInfo = {};

		PBRMaterialDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		PBRMaterialDescriptorSetAllocInfo.pNext = nullptr;
		PBRMaterialDescriptorSetAllocInfo.descriptorPool = vulkanRender->descriptorPool;		// 最多256种材质
		PBRMaterialDescriptorSetAllocInfo.descriptorSetCount = 1;
		PBRMaterialDescriptorSetAllocInfo.pSetLayouts = &sceneData->PBRMaterialDescriptor.layout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRender->device, &PBRMaterialDescriptorSetAllocInfo, &descriptorSet));

		VkDescriptorImageInfo baseColorInfo = {};
		baseColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		baseColorInfo.imageView = baseColor->textureImageView;
		baseColorInfo.sampler = baseColor->sampler;

		VkDescriptorImageInfo normalInfo = {};
		normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normalInfo.imageView = normal->textureImageView;
		normalInfo.sampler = normal->sampler;

		VkDescriptorImageInfo metallicRoughnessInfo = {};
		metallicRoughnessInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		metallicRoughnessInfo.imageView = metallicRoughness->textureImageView;
		metallicRoughnessInfo.sampler = metallicRoughness->sampler;

		std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = nullptr;
		descriptorWrites[0].pImageInfo = &baseColorInfo;
		descriptorWrites[0].pTexelBufferView = nullptr;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = nullptr;
		descriptorWrites[1].pImageInfo = &normalInfo;
		descriptorWrites[1].pTexelBufferView = nullptr;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = descriptorSet;
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pBufferInfo = nullptr;
		descriptorWrites[2].pImageInfo = &metallicRoughnessInfo;
		descriptorWrites[2].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(vulkanRender->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	void VulkanRenderSceneData::createDirectionalLightShadowDescriptorSet(VkImageView& directionalLightShadowView)
	{
		VkDescriptorSetLayoutBinding binding[2] = {};

		binding[0].binding = 0;
		binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding[0].descriptorCount = 1;
		binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		binding[1].binding = 1;
		binding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding[1].descriptorCount = 1;
		binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutCI = {};
		layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCI.pNext = nullptr;
		layoutCI.flags = 0;
		layoutCI.bindingCount = sizeof(binding) / sizeof(binding[0]);
		layoutCI.pBindings = binding;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRenderer->device, &layoutCI, nullptr, &directionalLightShadowDescriptor.layout));

		directionalLightShadowDescriptor.descriptorSet.resize(1);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = vulkanRenderer->descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &directionalLightShadowDescriptor.layout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRenderer->device, &descriptorSetAllocateInfo, &directionalLightShadowDescriptor.descriptorSet[0]));

		VkDescriptorImageInfo shadowImageInfo = {};
		shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shadowImageInfo.imageView = directionalLightShadowView;
		shadowImageInfo.sampler = vulkanRenderer->getOrCreateNearestSampler();

		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.offset = 0;
		bufferInfo.buffer = uniformShadowResource.buffer;
		bufferInfo.range = sizeof(UnifromBufferObjectShadowProjView);

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = directionalLightShadowDescriptor.descriptorSet[0];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = nullptr;
		descriptorWrites[0].pImageInfo = &shadowImageInfo;
		descriptorWrites[0].pTexelBufferView = nullptr;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].pNext = nullptr;
		descriptorWrites[1].dstSet = directionalLightShadowDescriptor.descriptorSet[0];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(vulkanRenderer->device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}

	Mesh* VulkanRenderSceneData::createCube()
	{
		Mesh* mesh = new Mesh();
		Node* node = new Node();
		node->worldTransform = glm::mat4(1.0f);
		node->localTransform = glm::mat4(1.0f);
		mesh->node = node;
		mesh->vertices =
		{
			// positions             // colors          // normals          // texture coords
			{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f},{0.0f,  0.0f, -1.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},

			{{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f},{0.0f,  0.0f,  1.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},

			{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f},{-1.0f,  0.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},

			{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f},{1.0f,  0.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},

			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f},{0.0f, -1.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},

			{{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{1.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{0.0f,  0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f},{0.0f,  1.0f,  0.0f},{0.0f,  1.0f}, {0.0f, 0.0f, 0.0f}}
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