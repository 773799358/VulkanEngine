#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include "vulkanRenderer.hpp"
#include "vulkanScene.hpp"

namespace VulkanEngine
{
	class VulkanRenderPass
	{
	public:
		struct VulkanFrameBufferAttachment
		{
			VkImage image;
			VkDeviceMemory memory;
			VkImageView imageView;
			VkFormat format;
		};

		struct VulkanFrameBuffer
		{
			int width;
			int height;
			VkFramebuffer frameBuffer;

			std::vector<VulkanFrameBufferAttachment> attachments;
		};

		struct VulkanDescriptor
		{
			VkDescriptorSetLayout layout;
			VkDescriptorSet descriptorSet;
		};

		struct VulkanRenderPipeline
		{
			VkPipelineLayout layout;
			VkPipeline pipeline;
		};

		VkRenderPass renderPass;
		std::vector<VulkanDescriptor> descriptorInfos;
		std::vector<VulkanRenderPipeline> renderPipelines;
		std::vector<VulkanFrameBuffer> frameBuffers;

		virtual void init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData);
		virtual void postInit() = 0;

		virtual void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize) = 0;
		virtual void draw(VkCommandBuffer commandBuffer, uint32_t vertexSize) = 0;
		virtual void clear() = 0;

	protected:
		VulkanRenderer* vulkanRender = nullptr;
		VulkanRenderSceneData* sceneData = nullptr;
	};
	
}