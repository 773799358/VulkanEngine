#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include "vulkanRenderer.hpp"
#include "vulkanRenderPass.hpp"

namespace VulkanEngine
{
	class UIPass : public VulkanRenderPass
	{
	public:
		void init(VulkanRenderer* vulkanRender, VulkanRenderPass* mainPass, uint32_t subpassIndex, VulkanRenderSceneData* sceneData);
		void postInit() override;

		void draw(VkCommandBuffer commandBuffer, uint32_t vertexSize) override;
		void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize) override;
		void clear() override;

	private:
		void uploadFonts();
		VulkanRenderPass* mainPass = nullptr;
	};

}