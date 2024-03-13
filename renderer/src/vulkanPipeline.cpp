#include "vulkanPipeline.hpp"
#include "macro.hpp"

namespace VulkanEngine
{
	void VulkanPipeline::createPipeline(VulkanRenderer* vulkanRender, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, std::vector<char>& vertShaderCode, std::vector<char>& fragShaderCode, std::vector<VkVertexInputBindingDescription>& bindingDescriptions, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkRenderPass& renderPass, uint32_t subpassIndex, VkViewport& viewport, VkRect2D& scissor, VkSampleCountFlagBits samples, std::vector<VkDynamicState>& dynamicStates, uint32_t attachmentCount, VkPipelineColorBlendAttachmentState* colorBlendAttachmentState, bool depthTest, bool depthWrite)
	{
		// shader
		// shaderModule只是字节码的容器，仅在渲染管线处理过程中需要，设置完就可以销毁
		VkShaderModule vertShaderModule = vulkanRender->createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = vulkanRender->createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageCI = {};
		vertShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageCI.module = vertShaderModule;
		vertShaderStageCI.pName = "main";
		vertShaderStageCI.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo fragShaderStageCI = {};
		fragShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageCI.module = fragShaderModule;
		fragShaderStageCI.pName = "main";
		fragShaderStageCI.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo shaderStageCI[] = { vertShaderStageCI, fragShaderStageCI };

		// 顶点数据描述
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = {};
		vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCI.vertexBindingDescriptionCount = bindingDescriptions.size();
		vertexInputStateCI.pVertexBindingDescriptions = bindingDescriptions.data();
		if (bindingDescriptions.empty())
		{
			vertexInputStateCI.pVertexAttributeDescriptions = nullptr;
		}
		vertexInputStateCI.vertexAttributeDescriptionCount = attributeDescriptions.size();
		vertexInputStateCI.pVertexAttributeDescriptions = attributeDescriptions.data();
		if (attributeDescriptions.empty())
		{
			vertexInputStateCI.pVertexAttributeDescriptions = nullptr;
		}

		// 图元
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {};
		inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

		// 视口与裁剪
		VkPipelineViewportStateCreateInfo viewportStateCI = {};
		viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCI.viewportCount = 1;
		viewportStateCI.pViewports = &viewport;
		viewportStateCI.scissorCount = 1;
		viewportStateCI.pScissors = &scissor;

		// 光栅化状态
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = {};
		rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCI.depthBiasClamp = VK_FALSE;
		rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.lineWidth = 1.0f;
		//rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateCI.depthBiasEnable = VK_FALSE;
		rasterizationStateCI.depthBiasConstantFactor = 0.0f;
		rasterizationStateCI.depthBiasClamp = 0.0f;
		rasterizationStateCI.depthBiasSlopeFactor = 0.0f;

		// 多重采样
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = {};
		multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCI.sampleShadingEnable = VK_FALSE;
		multisampleStateCI.rasterizationSamples = samples;
		multisampleStateCI.minSampleShading = 1.0f;
		multisampleStateCI.pSampleMask = nullptr;
		multisampleStateCI.alphaToCoverageEnable = VK_FALSE;
		multisampleStateCI.alphaToOneEnable = VK_FALSE;

		// color blend
		//std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
		//colorBlendAttachmentStates.resize(attachmentCount);
		//
		//for (size_t i = 0; i < colorBlendAttachmentStates.size(); i++)
		//{
		//	VkPipelineColorBlendAttachmentState& colorBlendAttachmentState = colorBlendAttachmentStates[i];
		//	colorBlendAttachmentState = {};
		//	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		//	colorBlendAttachmentState.blendEnable = VK_FALSE;	// 不开启会混合直接覆盖
		//	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		//	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		//	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		//	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		//}

		// 全局混合设置
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = {};
		colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendStateCI.logicOpEnable = VK_FALSE;
		colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
		colorBlendStateCI.attachmentCount = attachmentCount;
		colorBlendStateCI.pAttachments = colorBlendAttachmentState;
		colorBlendStateCI.blendConstants[0] = 0.0f;
		colorBlendStateCI.blendConstants[1] = 0.0f;
		colorBlendStateCI.blendConstants[2] = 0.0f;
		colorBlendStateCI.blendConstants[3] = 0.0f;

		// depth and stencil配置
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = {};
		depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateCI.depthTestEnable = depthTest;
		depthStencilStateCI.depthWriteEnable = depthWrite;
		depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS;
		
		if (depthTest == false && depthWrite == false)
		{
			depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
		}
		
		depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateCI.stencilTestEnable = VK_FALSE;

		// 动态项设置
		VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		//dynamicStateCI.dynamicStateCount = dynamicStates.size();		// TODO:暂不启用
		//dynamicStateCI.pDynamicStates = dynamicStates.data();
		//if (dynamicStates.empty())
		//{
		//	dynamicStateCI.pDynamicStates = nullptr;
		//}
		dynamicStateCI.dynamicStateCount = 0;
		dynamicStateCI.pDynamicStates = nullptr;

		VkGraphicsPipelineCreateInfo pipelineCI = {};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.stageCount = sizeof(shaderStageCI) / sizeof(shaderStageCI[0]);
		pipelineCI.pStages = shaderStageCI;
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		pipelineCI.pInputAssemblyState = &inputAssemblyCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.layout = pipelineLayout;
		pipelineCI.renderPass = renderPass;
		pipelineCI.subpass = subpassIndex;
		pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCI.pDynamicState = &dynamicStateCI;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanRender->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

		vkDestroyShaderModule(vulkanRender->device, vertShaderModule, nullptr);
		vkDestroyShaderModule(vulkanRender->device, fragShaderModule, nullptr);
	}
}