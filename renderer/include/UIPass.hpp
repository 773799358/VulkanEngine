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
		void init(VulkanRenderer* vulkanRender, VulkanRenderPass* mainPass);
		void postInit() override;

		void draw(VkCommandBuffer commandBuffer) override;
		void clear() override;

	private:
		void uploadFonts();
		VulkanRenderPass* mainPass = nullptr;
	};

}