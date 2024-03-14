#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include <array>
#include "camera.hpp"
#include "vulkanRenderer.hpp"
#include <map>

namespace VulkanEngine
{
	class VulkanRenderSceneData;

	// 注意16字节对齐

	struct VulkanResource
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct UniformBufferObjectVS
	{
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 proj = glm::mat4(1.0f);
	};

	struct UniformBufferObjectFS
	{
		glm::vec3 viewPos = glm::vec3(0.0f);
		float ambientStrength = 0.02f;
		glm::vec3 directionalLightPos = glm::vec3(1.0f);
		float padding0;
		glm::vec3 directionalLightColor = glm::vec3(1.0f);
		float padding1;
		glm::mat4 directionalLightProjView = glm::mat4(1.0f);
	};

	struct DeferredUniformBufferObject
	{
		glm::mat4 projView = glm::mat4(1.0f);
		UniformBufferObjectFS viewAndLight;
	};

	struct alignas(64) UniformBufferDynamicObject
	{
		glm::mat4 model = glm::mat4(1.0f);
	};

	struct UnifromBufferObjectShadowProjView
	{
		glm::mat4 projectView = glm::mat4(1.0f);
	};

	struct VulkanDescriptor
	{
		VkDescriptorSetLayout layout;
		std::vector<VkDescriptorSet> descriptorSet;			// 多个的设计是为了多帧用的
	};

	struct Vertex
	{
		glm::vec3 position;

		// 将position与其他属性进行分离，可以加速顶点着色
		glm::vec3 color = glm::vec3(1.0f);
		glm::vec3 normal;
		glm::vec2 texcoord;
		glm::vec3 tangent;

		static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

		bool operator== (const Vertex& other) const;
	};

	struct Texture
	{
		std::string path;
		std::string fullPath;
		VkImage textureImage;
		VkImageView textureImageView;
		VkDeviceMemory textureImageMemory;
		uint32_t mipLevels = 1;
		VkSampler sampler;

		void createTextureImage(VulkanRenderer* vulkanRender);
	};

	struct PBRMaterial
	{
		Texture* baseColor;
		Texture* metallicRoughness;
		Texture* normal;
		Texture* occlusion;
		Texture* emissive;
		
		//TODO:用统一属性和材质描述
		//VulkanResource materialUniform;
		VkDescriptorSet descriptorSet;

		void createDescriptorSet(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData);
	};

	struct Node
	{
		std::string name;
		glm::mat4 worldTransform = glm::mat4(1.0f);
		glm::mat4 localTransform = glm::mat4(1.0f);
		Node* parent = nullptr;
		std::vector<Node*> children;
	};

	struct Box
	{
		Box();

		glm::vec3 min;
		glm::vec3 max;

		void addPoint(const glm::vec3& point);
		void unionBox(const Box& box);
		glm::vec3 getCenter();
		glm::vec3 getSize();
		bool isValid() const;
	};

	struct Mesh
	{
		Node* node = nullptr;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		PBRMaterial* material = nullptr;
	};

	class VulkanRenderSceneData
	{
	public:
		void init(VulkanRenderer* vulkanRender);

		// 配置好场景数据后调用
		void setupRenderData();

		void createDirectionalLightShadowDescriptorSet(VkImageView& directionalLightShadowView);

		void createDeferredUniformDescriptorSet();

		// TODO:场景非uniform数据更新后续再处理
		void updateUniformRenderData();

		Box getSceneBounds();

		void lookAtSceneCenter();

		void clear();

		VulkanDescriptor uniformDescriptor;
		VulkanDescriptor PBRMaterialDescriptor;

		Node* rootNode = nullptr;
		std::vector<Node*> nodes;
		std::vector<Mesh*> meshes;
		std::vector<PBRMaterial*> materials;
		std::vector<Texture*> textures;

		std::string shaderName;
		std::string shaderVSFliePath;
		std::string shaderFSFilePath;

		std::string GBufferFSFilePath;

		std::string deferredLightingVSFilePath;
		std::string deferredLightingFSFilePath;

		std::string shadowVSFilePath;
		std::string shadowFSFilePath;

		CameraController cameraController;

		VulkanResource vertexResource;
		VulkanResource indexResource;

		VulkanResource uniformResource;
		UniformBufferObjectVS uniformBufferVSObject;
		UniformBufferObjectFS uniformBufferFSObject;

		VulkanResource uniformDynamicResource;
		std::vector<UniformBufferDynamicObject> uniformBufferDynamicObjects;

		VulkanResource uniformShadowResource;
		UnifromBufferObjectShadowProjView uniformBufferShadowVSObject;
		VulkanDescriptor directionalLightShadowDescriptor;

		VulkanResource deferredUniformResource;
		DeferredUniformBufferObject deferredUniformObject;
		VulkanDescriptor deferredUniformDescriptor;

		glm::mat4 rotate = glm::mat4(1.0);

	private:
		VulkanRenderer* vulkanRenderer = nullptr;

	public:

		void createVertexData();
		void createIndexData();

		void createUniformBufferData();
		void createUniformDescriptorSet();

		void createPBRDescriptorLayout();

		Mesh* createCube();
	};
}