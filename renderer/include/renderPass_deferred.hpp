#pragma once

#include "vulkanRenderer.hpp"
#include "vulkanRenderPass.hpp"
#include "vulkanScene.hpp"

namespace VulkanEngine
{
	class DeferredRenderPass : public VulkanRenderPass
	{
	public:
		void init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData) override;
		void postInit() override;

		void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize) override;
		void draw(VkCommandBuffer commandBuffer, uint32_t vertexSize) override;
		void clear() override;

		std::vector<VkFramebuffer> swapChainFrameBuffers;

	private:

		void setupAttachments();
		void setupRenderPass();
		void setupFrameBuffers();
		void setupDescriptorSetLayout();
		void setupPipelines();
		void setupDescriptorSet();
	};
}