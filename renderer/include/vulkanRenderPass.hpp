﻿#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include "vulkanRenderer.hpp"

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
		std::vector<VulkanDescriptor> descriptors;
		std::vector<VulkanRenderPipeline> renderPipelines;
		VulkanFrameBuffer frameBuffer;

		virtual void init(VulkanRenderer* vulkanRender);
		virtual void postInit() = 0;

		virtual void draw(VkCommandBuffer commandBuffer) = 0;
		virtual void clear() = 0;

	protected:
		VulkanRenderer* vulkanRender = nullptr;
	};

	class TestRenderPass : public VulkanRenderPass
	{
	public:
		void init(VulkanRenderer* vulkanRender) override;
		void postInit() override;

		void draw(VkCommandBuffer commandBuffer) override;
		void clear() override;

	private:
		void setupAttachments();
		void setupRenderPass();
		void setupDescriptorSetLayout();
		void setupPipelines();
		void setupDescriptorSet();
		void setupFramebufferDescriptorSet();
		void setupSwapChainFrameBuffers();

	// TODO:private
	public:
		std::vector<VkFramebuffer> swapChainFrameBuffers;
	};
}