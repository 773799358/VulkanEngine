#include "mainRenderPass.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"
#include "vulkanScene.hpp"

namespace VulkanEngine
{
	void MainRenderPass::init(VulkanRenderer* vulkanRender)
	{
		VulkanRenderPass::init(vulkanRender);
		setupRenderPass();
		setupDescriptorSetLayout();
		setupPipelines();
		setupFrameBuffers();
	}

	void MainRenderPass::postInit()
	{
	}

	void MainRenderPass::drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize)
	{
		vkCmdDrawIndexed(commandBuffer, indexSize, 1, 0, 0, 0);
	}

	void MainRenderPass::draw(VkCommandBuffer commandBuffer, uint32_t vertexSize)
	{
		vkCmdDraw(commandBuffer, vertexSize, 1, 0, 0);
	}

	void MainRenderPass::recreate()
	{
		for (const auto& frameBuffer : frameBuffers)
		{
			vkDestroyFramebuffer(vulkanRender->device, frameBuffer.frameBuffer, nullptr);
		}

		vkDestroyImage(vulkanRender->device, colorAttachment.image, nullptr);
		vkDestroyImageView(vulkanRender->device, colorAttachment.imageView, nullptr);
		vkFreeMemory(vulkanRender->device, colorAttachment.memory, nullptr);

		for (uint32_t i = 0; i < renderPipelines.size(); i++)
		{
			vkDestroyPipeline(vulkanRender->device, renderPipelines[i].pipeline, nullptr);
			vkDestroyPipelineLayout(vulkanRender->device, renderPipelines[i].layout, nullptr);
		}

		vkDestroyRenderPass(vulkanRender->device, renderPass, nullptr);

		setupRenderPass();
		setupPipelines();
		setupFrameBuffers();
	}

	void MainRenderPass::clear()
	{
		vkQueueWaitIdle(vulkanRender->graphicsQueue);
		for (const auto& frameBuffer : frameBuffers)
		{
			vkDestroyFramebuffer(vulkanRender->device, frameBuffer.frameBuffer, nullptr);
		}

		vkDestroyImage(vulkanRender->device, colorAttachment.image, nullptr);
		vkDestroyImageView(vulkanRender->device, colorAttachment.imageView, nullptr);
		vkFreeMemory(vulkanRender->device, colorAttachment.memory, nullptr);

		vkDestroyDescriptorSetLayout(vulkanRender->device, descriptorSetLayout, nullptr);

		for (uint32_t i = 0; i < renderPipelines.size(); i++)
		{
			vkDestroyPipeline(vulkanRender->device, renderPipelines[i].pipeline, nullptr);
			vkDestroyPipelineLayout(vulkanRender->device, renderPipelines[i].layout, nullptr);
		}

		vkDestroyRenderPass(vulkanRender->device, renderPass, nullptr);
	}

	void MainRenderPass::setupRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = vulkanRender->swapChainImageFormat;
		colorAttachment.samples = vulkanRender->msaaSamples;
		// color 渲染前清屏，渲染后保留内容
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// render pass 完成后请将 layout 转至 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 再输出
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// MSAA不能直接呈现

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;		// 指向第一个VkAttachmentDescription
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// 新提交的渲染任务需要将 layout 从 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 转为 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 再用于绘制。

		// 这里指定了depth的转换，所以不用像tutorial，create depth image的时候做转换
		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = vulkanRender->depthImageFormat;
		depthAttachment.samples = vulkanRender->msaaSamples;
		// depth 渲染前清屏，渲染后保留（除非后面不使用depth了，保守一些）
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve = {};
		colorAttachmentResolve.format = vulkanRender->swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentResolveRef = {};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// subpass
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;

		VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment, colorAttachmentResolve };

		// 配置依赖
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dependencyFlags = 0;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(vulkanRender->device, &renderPassInfo, nullptr, &renderPass));
	}

	void MainRenderPass::setupDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding[2] = {};
		uboLayoutBinding[0].binding = 0;
		uboLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding[0].descriptorCount = 1;
		uboLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;		// 仅在顶点着色器访问
		uboLayoutBinding[0].pImmutableSamplers = nullptr;

		uboLayoutBinding[1].binding = 1;
		uboLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		uboLayoutBinding[1].descriptorCount = 1;
		uboLayoutBinding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding[1].pImmutableSamplers = nullptr;

		//uboLayoutBinding[2].binding = 2;
		//uboLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		//uboLayoutBinding[2].descriptorCount = 1;
		//uboLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;		// 仅在片段着色器访问
		//uboLayoutBinding[2].pImmutableSamplers = nullptr

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = sizeof(uboLayoutBinding) / sizeof(uboLayoutBinding[0]);
		layoutInfo.pBindings = uboLayoutBinding;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRender->device, &layoutInfo, nullptr, &descriptorSetLayout));
	}

	void MainRenderPass::setupPipelines()
	{
		renderPipelines.resize(1);

		// 配置一次subpass的状态，DX内对应PipelineStateObject
		std::string shaderDir = vulkanRender->basePath + "spvs/";

		std::string vert = shaderDir + "vert.spv";
		auto vertShaderCode = VulkanUtil::readFile(vert);
		std::string frag = shaderDir + "frag.spv";
		auto fragShaderCode = VulkanUtil::readFile(frag);

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
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// 2.绘制的几何图元拓扑类型
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// 3.视窗和裁剪

		VkPipelineViewportStateCreateInfo viewportStateInfo = {};
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.pViewports = &vulkanRender->viewport;
		viewportStateInfo.scissorCount = 1;
		viewportStateInfo.pScissors = &vulkanRender->scissor;

		// 4.光栅化状态
		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {};
		rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateInfo.depthClampEnable = VK_FALSE;				// 超过远近平面的片元直接丢弃
		rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;		// true代表所有图元都不会进入光栅化，禁止任何输出到frameBuffer的方法
		rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;		// 填充，也可以绘制点、线
		rasterizationStateInfo.lineWidth = 1.0f;						// 大于1.0的线需要GPU wideLines支持
		rasterizationStateInfo.cullMode = VK_CULL_MODE_NONE;
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
		multisamplingInfo.rasterizationSamples = vulkanRender->msaaSamples;		// NxMSAA
		multisamplingInfo.minSampleShading = 1.0f;
		multisamplingInfo.pSampleMask = nullptr;
		multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
		multisamplingInfo.alphaToOneEnable = VK_FALSE;

		// 6.深度和模板测试
		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
		depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCreateInfo.depthTestEnable = VK_TRUE;
		depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; 
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

		// 7.color blend
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;	// 不开启会混合直接覆盖
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = 2;
		dynamicStateInfo.pDynamicStates = dynamicStates;

		// 9.管线布局
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

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
		pipelineInfo.pDepthStencilState = &depthStencilCreateInfo;
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

	void MainRenderPass::setupFrameBuffers()
	{
		const std::vector<VkImageView>& imageViews = vulkanRender->swapChainImageViews;
		frameBuffers.resize(imageViews.size());

		uint32_t swapChainWidth = vulkanRender->swapChainExtent.width;
		uint32_t swapChainHeight = vulkanRender->swapChainExtent.height;

		VkFormat format = vulkanRender->swapChainImageFormat;

		colorAttachment.format = format;

		vulkanRender->createImage(
			swapChainWidth,
			swapChainHeight,
			format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			colorAttachment.image,
			colorAttachment.memory,
			0,
			1,
			1,
			vulkanRender->msaaSamples);

		colorAttachment.imageView = vulkanRender->createImageView(colorAttachment.image, format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 1);

		vulkanRender->transitionImageLayout(colorAttachment.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);

		for (size_t i = 0; i < frameBuffers.size(); i++)
		{
			VkImageView attachments[] =
			{
				colorAttachment.imageView,
				vulkanRender->depthImageView,
				vulkanRender->swapChainImageViews[i]
			};

			VkFramebufferCreateInfo frameBufferInfo = {};
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = renderPass;	// 兼容就行，需要相同的附件数量和类型
			frameBufferInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
			frameBufferInfo.pAttachments = attachments;
			frameBufferInfo.width = swapChainWidth;
			frameBufferInfo.height = swapChainHeight;
			frameBufferInfo.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(vulkanRender->device, &frameBufferInfo, nullptr, &frameBuffers[i].frameBuffer));
		}
	}

}