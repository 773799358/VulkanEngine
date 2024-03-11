#include "shadowRenderPass.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"

namespace VulkanEngine
{
	void DirectionalLightShadowMapRenderPass::init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData)
	{
		VulkanRenderPass::init(vulkanRender, sceneData);

		setupAttachments();
		setupRenderPass();
		setupFrameBuffers();
		setupDescriptorSetLayout();
		setupPipelines();
		setupDescriptorSet();
	}

	void DirectionalLightShadowMapRenderPass::postInit()
	{
	}

	void DirectionalLightShadowMapRenderPass::drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize)
	{
		vkCmdDrawIndexed(commandBuffer, indexSize, 1, 0, 0, 0);
	}

	void DirectionalLightShadowMapRenderPass::draw(VkCommandBuffer commandBuffer, uint32_t vertexSize)
	{
	}

	void DirectionalLightShadowMapRenderPass::clear()
	{
		vkQueueWaitIdle(vulkanRender->graphicsQueue);
		for (const auto& frameBuffer : frameBuffers)
		{
			vkDestroyFramebuffer(vulkanRender->device, frameBuffer.frameBuffer, nullptr);
			for (const auto& attachment : frameBuffer.attachments)
			{
				vkDestroyImage(vulkanRender->device, attachment.image, nullptr);
				vkDestroyImageView(vulkanRender->device, attachment.imageView, nullptr);
				vkFreeMemory(vulkanRender->device, attachment.memory, nullptr);
			}
		}

		for (uint32_t i = 0; i < renderPipelines.size(); i++)
		{
			vkDestroyPipeline(vulkanRender->device, renderPipelines[i].pipeline, nullptr);
			vkDestroyPipelineLayout(vulkanRender->device, renderPipelines[i].layout, nullptr);
		}

		vkDestroyDescriptorSetLayout(vulkanRender->device, descriptor.layout, nullptr);
		vkFreeDescriptorSets(vulkanRender->device, vulkanRender->descriptorPool, 1, &descriptor.descriptorSet);

		vkDestroyRenderPass(vulkanRender->device, renderPass, nullptr);
	}

	void DirectionalLightShadowMapRenderPass::setupAttachments()
	{
		frameBuffers.resize(1);
		auto& mainFrameBuffer = frameBuffers[0];
		mainFrameBuffer.width = shadowMapSize;
		mainFrameBuffer.height = shadowMapSize;
		mainFrameBuffer.attachments.resize(2);

		// color
		mainFrameBuffer.attachments[0].format = VK_FORMAT_R32_SFLOAT;
		vulkanRender->createImage(shadowMapSize, shadowMapSize,
			mainFrameBuffer.attachments[0].format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mainFrameBuffer.attachments[0].image,
			mainFrameBuffer.attachments[0].memory,
			0,		// 没有特殊用法，就传0
			1,
			1);

		mainFrameBuffer.attachments[0].imageView = vulkanRender->createImageView(mainFrameBuffer.attachments[0].image, mainFrameBuffer.attachments[0].format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 1);

		// depth
		mainFrameBuffer.attachments[1].format = vulkanRender->depthImageFormat;
		vulkanRender->createImage(shadowMapSize, shadowMapSize,
			mainFrameBuffer.attachments[1].format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,	// 用一次就不用了
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mainFrameBuffer.attachments[1].image,
			mainFrameBuffer.attachments[1].memory,
			0,
			1,
			1);

		mainFrameBuffer.attachments[1].imageView = vulkanRender->createImageView(mainFrameBuffer.attachments[1].image, mainFrameBuffer.attachments[1].format, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 1);
	}

	void DirectionalLightShadowMapRenderPass::setupRenderPass()
	{
		auto& mainFrameBuffer = frameBuffers[0];

		VkAttachmentDescription attachment[2] = {};

		attachment[0].format = mainFrameBuffer.attachments[0].format;
		attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachment[1].format = mainFrameBuffer.attachments[1].format;
		attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;		// 说不定格式有stencil，所以不能只depth optimal


		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthRef = {};
		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subPasses[1] = {};
		subPasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subPasses[0].colorAttachmentCount = 1;
		subPasses[0].pColorAttachments = &colorRef;
		subPasses[0].pDepthStencilAttachment = &depthRef;

		VkSubpassDependency dependency[2] = {};
		dependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency[0].dstSubpass = 0;
		dependency[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;		// 等待之前的pass哪个阶段执行哪个操作
		dependency[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;		// 之后我们进行哪个阶段哪个操作
		dependency[0].srcAccessMask = 0;												// 0代表读写不关心，阶段执行完就行
		dependency[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency[0].dependencyFlags = 0;

		dependency[1].srcSubpass = 0;
		dependency[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;				// color out阶段的写操作结束，管线就结束了
		dependency[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency[1].dstAccessMask = 0;
		dependency[1].dependencyFlags = 0;

		VkRenderPassCreateInfo renderPassCI = {};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = (sizeof(attachment) / sizeof(attachment[0]));
		renderPassCI.pAttachments = attachment;
		renderPassCI.subpassCount = (sizeof(subPasses) / sizeof(subPasses[0]));
		renderPassCI.pSubpasses = subPasses;
		renderPassCI.dependencyCount = (sizeof(dependency) / sizeof(dependency[0]));
		renderPassCI.pDependencies = dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(vulkanRender->device, &renderPassCI, nullptr, &renderPass));
	}

	void DirectionalLightShadowMapRenderPass::setupFrameBuffers()
	{
		VkImageView attachments[2] = { frameBuffers[0].attachments[0].imageView, frameBuffers[0].attachments[1].imageView };

		VkFramebufferCreateInfo frameBufferCI = {};
		frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCI.flags = 0;
		frameBufferCI.renderPass = renderPass;
		frameBufferCI.attachmentCount = (sizeof(attachments) / sizeof(attachments[0]));
		frameBufferCI.pAttachments = attachments;
		frameBufferCI.width = frameBuffers[0].width;
		frameBufferCI.height = frameBuffers[0].height;
		frameBufferCI.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(vulkanRender->device, &frameBufferCI, nullptr, &frameBuffers[0].frameBuffer));
	}

	void DirectionalLightShadowMapRenderPass::setupDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding binding[2] = {};

		binding[0].binding = 0;
		binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding[0].descriptorCount = 1;
		binding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		binding[1].binding = 1;
		binding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		binding[1].descriptorCount = 1;
		binding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutCI = {};
		layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCI.pNext = nullptr;
		layoutCI.flags = 0;
		layoutCI.bindingCount = sizeof(binding) / sizeof(binding[0]);
		layoutCI.pBindings = binding;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRender->device, &layoutCI, nullptr, &descriptor.layout));
	}

	void DirectionalLightShadowMapRenderPass::setupPipelines()
	{
		renderPipelines.resize(1);

		// layout
		VkDescriptorSetLayout descriptorSetLayouts[] = { descriptor.layout };

		VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
		pipelineLayoutCI.pSetLayouts = descriptorSetLayouts;

		VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanRender->device, &pipelineLayoutCI, nullptr, &renderPipelines[0].layout));

		// shaders
		// 配置一次subpass的状态，DX内对应PipelineStateObject
		auto vertShaderCode = VulkanUtil::readFile(sceneData->shadowVSFilePath);
		auto fragShaderCode = VulkanUtil::readFile(sceneData->shadowFSFilePath);

		// shaderModule只是字节码的容器，仅在渲染管线处理过程中需要，设置完就可以销毁
		VkShaderModule vertShaderModule = vulkanRender->createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = vulkanRender->createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageCI = {};
		vertShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageCI.module = vertShaderModule;
		vertShaderStageCI.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageCI = {};
		fragShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageCI.module = fragShaderModule;
		fragShaderStageCI.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStageCI[] = { vertShaderStageCI, fragShaderStageCI };

		// 顶点数据描述
		auto vertexBindingDescriptions = Vertex::getBindingDescriptions();
		auto vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = {};
		vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexBindingDescriptions[0];
		vertexInputStateCI.vertexAttributeDescriptionCount = 1;
		vertexInputStateCI.pVertexAttributeDescriptions = &vertexAttributeDescriptions[0];

		// 图元
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {};
		inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

		// 视口与裁剪
		VkViewport viewport = { 0, 0, frameBuffers[0].width, frameBuffers[0].height, 0.0, 1.0 };
		VkRect2D scissor = { {0, 0}, { frameBuffers[0].width, frameBuffers[0].height } };

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
		rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateCI.depthBiasEnable = VK_FALSE;
		rasterizationStateCI.depthBiasConstantFactor = 0.0f;
		rasterizationStateCI.depthBiasClamp = 0.0f;
		rasterizationStateCI.depthBiasSlopeFactor = 0.0f;

		// 多重采样
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = {};
		multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCI.sampleShadingEnable = VK_FALSE;
		multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// color blend
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
		colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentState.blendEnable = VK_FALSE;	// 不开启会混合直接覆盖
		colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		// 全局混合设置
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = {};
		colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendStateCI.logicOpEnable = VK_FALSE;
		colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
		colorBlendStateCI.attachmentCount = 1;
		colorBlendStateCI.pAttachments = &colorBlendAttachmentState;
		colorBlendStateCI.blendConstants[0] = 0.0f;
		colorBlendStateCI.blendConstants[1] = 0.0f;
		colorBlendStateCI.blendConstants[2] = 0.0f;
		colorBlendStateCI.blendConstants[3] = 0.0f;

		// depth and stencil配置
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = {};
		depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateCI.depthTestEnable = VK_TRUE;
		depthStencilStateCI.depthWriteEnable = VK_TRUE;
		depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateCI.stencilTestEnable = VK_FALSE;

		// 动态项设置
		VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
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
		pipelineCI.layout = renderPipelines[0].layout;
		pipelineCI.renderPass = renderPass;
		pipelineCI.subpass = 0;
		pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCI.pDynamicState = &dynamicStateCI;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanRender->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &renderPipelines[0].pipeline));

		vkDestroyShaderModule(vulkanRender->device, vertShaderModule, nullptr);
		vkDestroyShaderModule(vulkanRender->device, fragShaderModule, nullptr);
	}

	void DirectionalLightShadowMapRenderPass::setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = vulkanRender->descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptor.layout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRender->device, &descriptorSetAllocateInfo, &descriptor.descriptorSet));

		VkDescriptorBufferInfo uniformBufferInfo[2] = {};
		uniformBufferInfo[0].offset = 0;
		uniformBufferInfo[0].buffer = sceneData->uniformShadowResource.buffer;
		uniformBufferInfo[0].range = sizeof(UnifromBufferObjectShadowVS);

		uniformBufferInfo[1].offset = 0;
		uniformBufferInfo[1].buffer = sceneData->uniformDynamicResource.buffer;
		uniformBufferInfo[1].range = sizeof(UniformBufferDynamicObject);
		
		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].pNext = nullptr;
		descriptorWrites[0].dstSet = descriptor.descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &uniformBufferInfo[0];

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].pNext = nullptr;
		descriptorWrites[1].dstSet = descriptor.descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &uniformBufferInfo[1];

		vkUpdateDescriptorSets(vulkanRender->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

}