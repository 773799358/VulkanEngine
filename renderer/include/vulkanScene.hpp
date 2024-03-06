#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include <array>
#include "camera.hpp"
#include "vulkanRenderer.hpp"

namespace VulkanEngine
{
	// 注意16字节对齐

	struct VulkanResource
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct UniformBufferObject
	{
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 proj = glm::mat4(1.0f);
	};

	struct alignas(64) UniformBufferDynamicObject
	{
		glm::mat4 model = glm::mat4(1.0f);
	};

	struct VulkanDescriptor
	{
		VkDescriptorSetLayout layout;
		std::vector<VkDescriptorSet> descriptorSet;
	};

	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 color;
		glm::vec3 normal;
		glm::vec2 texcoord;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();

		bool operator== (const Vertex& other) const;
	};

	struct Material
	{

	};

	struct Node
	{
		glm::mat4 transform;
	};

	struct StaticMesh
	{
		Node* node = nullptr;
		std::vector<Vertex> vertices;
		VulkanResource vertexBuffer;

		std::vector<uint32_t> indices;
		VulkanResource indexBuffer;
	};

	struct Texture
	{

	};

	struct PointLight
	{

	};

	struct DirectionalLight
	{

	};

	class VulkanRenderSceneData
	{
	public:
		void init(VulkanRenderer* vulkanRender);

		// 配置好场景数据后调用
		void setupRenderData();

		// TODO:场景非uniform数据更新后续再处理
		void updateUniformRenderData();

		void clear();

		VulkanDescriptor descriptor;

		std::vector<Node*> nodes;
		std::vector<StaticMesh*> meshes;
		std::vector<Material*> materials;
		std::vector<Texture*> textures;
		std::vector<PointLight*> pointLights;
		std::vector<DirectionalLight*> directionalLights;

		CameraController cameraController;

		VulkanResource uniformResource;
		UniformBufferObject uniformBufferObject;

		VulkanResource uniformDynamicResource;
		std::vector<UniformBufferDynamicObject> uniformBufferDynamicObjects;

	private:
		VulkanRenderer* vulkanRenderer = nullptr;

		//VkSampler nearestSampler = VK_NULL_HANDLE;
		VkSampler linearSampler = VK_NULL_HANDLE;

	public:
		//VkSampler getNearestSampler();
		VkSampler getLinearSampler();

		void createVertexData(StaticMesh* mesh);
		void createIndexData(StaticMesh* mesh);

		void createUniformBufferData();
		void createDescriptorSet();

		StaticMesh* createCube();
	};
}