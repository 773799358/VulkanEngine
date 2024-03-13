#include "renderPass_forwardLight.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"
#include "vulkanScene.hpp"
#include "vulkanPipeline.hpp"

namespace VulkanEngine
{
	void MainRenderPass::init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData)
	{
		VulkanRenderPass::init(vulkanRender, sceneData);
		setupRenderPass();
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

	void MainRenderPass::setupPipelines()
	{
		renderPipelines.resize(1);

		std::array<VkDescriptorSetLayout, 3> descriptorSetLayout = { sceneData->uniformDescriptor.layout, sceneData->PBRMaterialDescriptor.layout, sceneData->directionalLightShadowDescriptor.layout };

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = descriptorSetLayout.size();
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayout.data();
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanRender->device, &pipelineLayoutInfo, nullptr, &renderPipelines[0].layout));

		// 配置一次subpass的状态，DX内对应PipelineStateObject
		auto vertShaderCode = VulkanUtil::readFile(sceneData->shaderVSFliePath);
		auto fragShaderCode = VulkanUtil::readFile(sceneData->shaderFSFilePath);

		auto bindingDescriptions = Vertex::getBindingDescriptions();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		std::vector<VkDynamicState> dynamicStates =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		std::array<VkPipelineColorBlendAttachmentState, 1> colorBlendAttachmentState = {};
		colorBlendAttachmentState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentState[0].blendEnable = VK_FALSE;

		VulkanPipeline::createPipeline(vulkanRender, renderPipelines[0].pipeline,
			renderPipelines[0].layout,
			vertShaderCode, fragShaderCode,
			bindingDescriptions, attributeDescriptions,
			renderPass,
			0,
			vulkanRender->viewport, vulkanRender->scissor,
			vulkanRender->msaaSamples,
			dynamicStates, 1, colorBlendAttachmentState.data(), true, true);
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