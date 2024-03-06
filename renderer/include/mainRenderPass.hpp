#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include "vulkanRenderer.hpp"
#include "vulkanRenderPass.hpp"

namespace VulkanEngine
{
	class MainRenderPass : public VulkanRenderPass
	{
	public:
		void init(VulkanRenderer* vulkanRender) override;
		void postInit() override;

		void draw(VkCommandBuffer commandBuffer) override;
		void recreate();
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
		VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	};
}