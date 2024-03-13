#pragma once

#include "vulkan/vulkan.h"
#include "vulkanRenderer.hpp"
#include <vector>

namespace VulkanEngine
{
	class VulkanPipeline
	{
	public:
		// TODO:暂时只支持非透明，后面需要再继续加参数
		static void createPipeline(
			VulkanRenderer* vulkanRender,
			VkPipeline& pipeline,
			VkPipelineLayout& pipelineLayout, 
			std::vector<char>& vertShaderCode, 
			std::vector<char>& fragShaderCode, 
			std::vector<VkVertexInputBindingDescription>& bindingDescriptions,
			std::vector<VkVertexInputAttributeDescription>& attributeDescriptions,
			VkRenderPass& renderPass, 
			uint32_t subpassIndex, 
			VkViewport& viewport, 
			VkRect2D& scissor,
			VkSampleCountFlagBits samples,
			std::vector<VkDynamicState>& dynamicStates,
			uint32_t attachmentCount,
			VkPipelineColorBlendAttachmentState* colorBlendAttachmentState,
			bool depthTest, bool depthWrite);
	};
}