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

		void draw(VkCommandBuffer commandBuffer, uint32_t vertexSize) override;
		void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize) override;
		void recreate();
		void clear() override;

	private:
		void setupRenderPass();
		void setupDescriptorSetLayout();
		void setupPipelines();
		void setupFrameBuffers();

		VulkanFrameBufferAttachment colorAttachment;
	};
}