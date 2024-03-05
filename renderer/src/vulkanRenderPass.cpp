#include "vulkanRenderPass.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"

namespace VulkanEngine
{
	void VulkanRenderPass::init(VulkanRenderer* vulkanRender)
	{
		this->vulkanRender = vulkanRender;
	}

	void TestRenderPass::init(VulkanRenderer* vulkanRender)
	{
		VulkanRenderPass::init(vulkanRender);
		setupAttachments();
		setupRenderPass();
		setupDescriptorSetLayout();
		setupPipelines();
		setupDescriptorSet();
		setupFramebufferDescriptorSet();
		setupSwapChainFrameBuffers();
	}

	void TestRenderPass::postInit()
	{
	}

	void TestRenderPass::draw(VkCommandBuffer commandBuffer)
	{
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	}

	void TestRenderPass::clear()
	{
		vkQueueWaitIdle(vulkanRender->graphicsQueue);
		vkQueueWaitIdle(vulkanRender->presentQueue);
		for (const auto& frameBuffer : swapChainFrameBuffers)
		{
			vkDestroyFramebuffer(vulkanRender->device, frameBuffer, nullptr);
		}

		for (uint32_t i = 0; i < renderPipelines.size(); i++)
		{
			vkDestroyPipeline(vulkanRender->device, renderPipelines[i].pipeline, nullptr);
			vkDestroyPipelineLayout(vulkanRender->device, renderPipelines[i].layout, nullptr);
		}

		vkDestroyRenderPass(vulkanRender->device, renderPass, nullptr);
	}

	void TestRenderPass::setupAttachments()
	{
		
	}

	void TestRenderPass::setupRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = vulkanRender->swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;	// 不做多重采样
		// color and depth 渲染前清屏，渲染后保留内容
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// stencil
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// render pass 完成后请将 layout 转至 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 再输出

		// subpass
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;		// 指向第一个VkAttachmentDescription
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// 新提交的渲染任务需要将 layout 从 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 转为 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 再用于绘制。

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		// 配置依赖
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(vulkanRender->device, &renderPassInfo, nullptr, &renderPass));
	}

	void TestRenderPass::setupDescriptorSetLayout()
	{
	}

	void TestRenderPass::setupPipelines()
	{
		// 配置一次subpass的状态，DX内对应PipelineStateObject
		std::string shaderDir = vulkanRender->basePath + "spvs/";
		std::string vert = shaderDir + "vert.spv";
		auto vertShaderCode = vulkanUtil::readFile(vert);

		std::string frag = shaderDir + "frag.spv";
		auto fragShaderCode = vulkanUtil::readFile(frag);

		// shaderModule只是字节码的容器，仅在渲染管线处理过程中需要，设置完就可以销毁
		VkShaderModule vertShaderModule = vulkanRender->createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = vulkanRender->createShaderModule(fragShaderCode);

		// 真正和pipeline产生联系的是shaderStage
		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";				// 入口函数

		// 设置常量，可以编译阶段进行优化
		//vertShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";				// 入口函数

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// 1.输入的顶点数据描述
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;		// 暂时不需要顶点数据
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;

		// 2.绘制的几何图元拓扑类型
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// 3.视窗和裁剪
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)vulkanRender->swapChainExtent.width;
		viewport.height = (float)vulkanRender->swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};	// 裁剪外的区域会被丢弃，其实就是个过滤器
		scissor.offset = { 0, 0 };
		scissor.extent = vulkanRender->swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportStateInfo = {};
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.pViewports = &viewport;
		viewportStateInfo.scissorCount = 1;
		viewportStateInfo.pScissors = &scissor;

		// 4.光栅化状态
		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {};
		rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateInfo.depthClampEnable = VK_FALSE;				// 超过远近平面的片元直接丢弃
		rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;		// true代表所有图元都不会进入光栅化，禁止任何输出到frameBuffer的方法
		rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;		// 填充，也可以绘制点、线
		rasterizationStateInfo.lineWidth = 1.0f;						// 大于1.0的线需要GPU wideLines支持
		rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;		// 顺时针为正，这里要注意，vulkan的NDC，Y是向下的
		// 暂时不用深度偏移
		rasterizationStateInfo.depthBiasEnable = VK_FALSE;
		rasterizationStateInfo.depthBiasConstantFactor = 0.0f;
		rasterizationStateInfo.depthBiasClamp = 0.0f;
		rasterizationStateInfo.depthBiasSlopeFactor = 0.0f;

		// 5.重采样
		// 暂时关闭MSAA
		VkPipelineMultisampleStateCreateInfo multisamplingInfo = {};
		multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingInfo.sampleShadingEnable = VK_FALSE;
		multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;		// NxMSAA
		multisamplingInfo.minSampleShading = 1.0f;
		multisamplingInfo.pSampleMask = nullptr;
		multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
		multisamplingInfo.alphaToOneEnable = VK_FALSE;

		// 6.深度和模板测试
		// 暂时不用

		// 7.color blend
		/*
			if (blendEnable) {
				finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
				finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
			} else {
				finalColor = newColor;
			}

			finalColor = finalColor & colorWriteMask;
		*/

		// 大多数情况的混合：
		// finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
		// finalColor.a = newAlpha.a;
		// 
		// colorBlendAttachment.blendEnable = VK_TRUE;
		// colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		// colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		// colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		// colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		// colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		// colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;	// 不开启混合直接覆盖
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		// 全局混合设置（关闭，开启后会自动禁止attachment的color blend）
		VkPipelineColorBlendStateCreateInfo colorBlendingStateInfo = {};
		colorBlendingStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendingStateInfo.logicOpEnable = VK_FALSE;
		colorBlendingStateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendingStateInfo.attachmentCount = 1;
		colorBlendingStateInfo.pAttachments = &colorBlendAttachment;
		colorBlendingStateInfo.blendConstants[0] = 0.0f;
		colorBlendingStateInfo.blendConstants[1] = 0.0f;
		colorBlendingStateInfo.blendConstants[2] = 0.0f;
		colorBlendingStateInfo.blendConstants[3] = 0.0f;

		// 8.动态部分
		VkDynamicState dynamicStates[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = 2;
		dynamicStateInfo.pDynamicStates = dynamicStates;

		// 9.管线布局
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		renderPipelines.resize(1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanRender->device, &pipelineLayoutInfo, nullptr, &renderPipelines[0].layout));

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizationStateInfo;
		pipelineInfo.pMultisampleState = &multisamplingInfo;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &colorBlendingStateInfo;
		pipelineInfo.pDynamicState = nullptr;

		pipelineInfo.layout = renderPipelines[0].layout;

		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;

		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanRender->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &renderPipelines[0].pipeline));

		vkDestroyShaderModule(vulkanRender->device, fragShaderModule, nullptr);
		vkDestroyShaderModule(vulkanRender->device, vertShaderModule, nullptr);
	}

	void TestRenderPass::setupDescriptorSet()
	{
	}

	void TestRenderPass::setupFramebufferDescriptorSet()
	{
	}

	void TestRenderPass::setupSwapChainFrameBuffers()
	{
		swapChainFrameBuffers.resize(vulkanRender->swapChainImageViews.size());

		for (size_t i = 0; i < swapChainFrameBuffers.size(); i++)
		{
			VkImageView attachments[] =
			{
				vulkanRender->swapChainImageViews[i]
			};

			VkFramebufferCreateInfo frameBufferInfo = {};
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = renderPass;	// 兼容就行，需要相同的附件数量和类型
			frameBufferInfo.attachmentCount = 1;
			frameBufferInfo.pAttachments = attachments;
			frameBufferInfo.width = vulkanRender->swapChainExtent.width;
			frameBufferInfo.height = vulkanRender->swapChainExtent.height;
			frameBufferInfo.layers = 1;

			if (vkCreateFramebuffer(vulkanRender->device, &frameBufferInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

}