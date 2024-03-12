#pragma once

#include "vulkanRenderPass.hpp"

namespace VulkanEngine
{
	class DirectionalLightShadowMapRenderPass : public VulkanRenderPass
	{
	public:
		void init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData) override;
		void postInit() override;

		void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize) override;
		void draw(VkCommandBuffer commandBuffer, uint32_t vertexSize) override;
		void clear() override;

	private:
		void setupAttachments();
		void setupRenderPass();
		void setupPipelines();
		void setupFrameBuffers();
		void setupDescriptorSetLayout();
		void setupDescriptorSet();

		uint32_t shadowMapSize = 4096;

		VulkanFrameBufferAttachment depthAttachment;
	};
}