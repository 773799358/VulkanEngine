#include "renderPass_deferred.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"
#include "vulkanPipeline.hpp"

namespace VulkanEngine
{
	void DeferredRenderPass::init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData)
	{
		VulkanRenderPass::init(vulkanRender, sceneData);

		descriptorInfos.resize(1);

		setupAttachments();
		setupDescriptorSetLayout();
		setupDescriptorSet();
		setupRenderPass();
		setupFrameBuffers();
		setupPipelines();
	}

	void DeferredRenderPass::postInit()
	{

	}

	void DeferredRenderPass::drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexSize)
	{
		vkCmdDrawIndexed(commandBuffer, indexSize, 1, 0, 0, 0);
	}

	void DeferredRenderPass::draw(VkCommandBuffer commandBuffer, uint32_t vertexSize)
	{
		vkCmdDraw(commandBuffer, vertexSize, 1, 0, 0);
	}

	void DeferredRenderPass::clear()
	{
		vkQueueWaitIdle(vulkanRender->graphicsQueue);
		for (const auto& frameBuffer : frameBuffers)
		{
			for (const auto& attachment : frameBuffer.attachments)
			{
				vkDestroyImage(vulkanRender->device, attachment.image, nullptr);
				vkDestroyImageView(vulkanRender->device, attachment.imageView, nullptr);
				vkFreeMemory(vulkanRender->device, attachment.memory, nullptr);
			}
		}

		for (const auto& frameBuffer : swapChainFrameBuffers)
		{
			vkDestroyFramebuffer(vulkanRender->device, frameBuffer, nullptr);
		}

		for (uint32_t i = 0; i < renderPipelines.size(); i++)
		{
			vkDestroyPipeline(vulkanRender->device, renderPipelines[i].pipeline, nullptr);
			vkDestroyPipelineLayout(vulkanRender->device, renderPipelines[i].layout, nullptr);
		}

		vkDestroyDescriptorSetLayout(vulkanRender->device, descriptorInfos[0].layout, nullptr);
		vkFreeDescriptorSets(vulkanRender->device, vulkanRender->descriptorPool, 1, &descriptorInfos[0].descriptorSet);

		vkDestroyRenderPass(vulkanRender->device, renderPass, nullptr);
	}

	void DeferredRenderPass::recreate()
	{
		for (const auto& frameBuffer : frameBuffers)
		{
			for (const auto& attachment : frameBuffer.attachments)
			{
				vkDestroyImage(vulkanRender->device, attachment.image, nullptr);
				vkDestroyImageView(vulkanRender->device, attachment.imageView, nullptr);
				vkFreeMemory(vulkanRender->device, attachment.memory, nullptr);
			}
		}

		for (const auto& frameBuffer : swapChainFrameBuffers)
		{
			vkDestroyFramebuffer(vulkanRender->device, frameBuffer, nullptr);
		}

		for (uint32_t i = 0; i < renderPipelines.size(); i++)
		{
			vkDestroyPipeline(vulkanRender->device, renderPipelines[i].pipeline, nullptr);
			vkDestroyPipelineLayout(vulkanRender->device, renderPipelines[i].layout, nullptr);
		}

		setupAttachments();
		setupRenderPass();
		setupFrameBuffers();
		setupPipelines();
	}

	void DeferredRenderPass::setupAttachments()
	{
		frameBuffers.resize(1);
		// 分别为normal，材质相关系数，baseColor
		frameBuffers[0].attachments.resize(3);
		
		auto& mainFrameBuffer = frameBuffers[0];

		mainFrameBuffer.attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
		mainFrameBuffer.attachments[1].format = VK_FORMAT_R8G8B8A8_UNORM;
		mainFrameBuffer.attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;

		uint32_t width = vulkanRender->swapChainExtent.width;
		uint32_t height = vulkanRender->swapChainExtent.height;

		// TODO:VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		for (size_t i = 0; i < mainFrameBuffer.attachments.size(); i++)
		{
			vulkanRender->createImage(width, height, mainFrameBuffer.attachments[i].format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,		// 后续pass，image会作为输入	
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				mainFrameBuffer.attachments[i].image,
				mainFrameBuffer.attachments[i].memory,
				0, 1, 1);

			mainFrameBuffer.attachments[i].imageView = vulkanRender->createImageView(mainFrameBuffer.attachments[i].image,
				mainFrameBuffer.attachments[i].format,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_VIEW_TYPE_2D,
				1,
				1);
		}
	}

	void DeferredRenderPass::setupRenderPass()
	{
		auto& mainFrameBuffer = frameBuffers[0];

		// attachments
		// 0.normal 1.MR 2.albedo 3.depth 4.present
		std::array<VkAttachmentDescription, 5> attachments = {};
		{
			VkAttachmentDescription& gbufferNormalAttachmentDescription = attachments[0];
			gbufferNormalAttachmentDescription.format = mainFrameBuffer.attachments[0].format;
			gbufferNormalAttachmentDescription.samples = vulkanRender->msaaSamples;
			gbufferNormalAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferNormalAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		// TODO:STORE
			gbufferNormalAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			gbufferNormalAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferNormalAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			gbufferNormalAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentDescription& gbufferMetallicRoughnessShadingIDAttachmentDescription = attachments[1];
			gbufferMetallicRoughnessShadingIDAttachmentDescription.format = mainFrameBuffer.attachments[1].format;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.samples = vulkanRender->msaaSamples;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			gbufferMetallicRoughnessShadingIDAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentDescription& gbufferAlbedoAttachmentDescription = attachments[2];
			gbufferAlbedoAttachmentDescription.format = mainFrameBuffer.attachments[2].format;
			gbufferAlbedoAttachmentDescription.samples = vulkanRender->msaaSamples;
			gbufferAlbedoAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferAlbedoAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferAlbedoAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			gbufferAlbedoAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferAlbedoAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			gbufferAlbedoAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentDescription& depthAttachmentDescription = attachments[3];
			depthAttachmentDescription.format = vulkanRender->depthImageFormat;
			depthAttachmentDescription.samples = vulkanRender->msaaSamples;
			depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;	// TODO:STORE
			depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription& swapChainImageAttachmentDescription = attachments[4];
			swapChainImageAttachmentDescription.format = vulkanRender->swapChainImageFormat;
			swapChainImageAttachmentDescription.samples = vulkanRender->msaaSamples;
			swapChainImageAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			swapChainImageAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			swapChainImageAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			swapChainImageAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			swapChainImageAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			swapChainImageAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}

		// subpass
		std::array<VkSubpassDescription, 2> subpasses = {};
		std::array<VkSubpassDependency, 3> dependencies = {};

		// gbuffer pass
		std::array<VkAttachmentReference, 3> gbufferAttachmentReference = {};
		// lighting pass
		std::array<VkAttachmentReference, 4> deferredLightingPassInputAttachementReference = {};

		std::array<VkAttachmentReference, 1> deferredLightingPassOutputColorAttachmentReference = {};
		{
			gbufferAttachmentReference[0].attachment = 0;
			gbufferAttachmentReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			
			gbufferAttachmentReference[1].attachment = 1;
			gbufferAttachmentReference[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			gbufferAttachmentReference[2].attachment = 2;
			gbufferAttachmentReference[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthAttachmentReference = {};
			depthAttachmentReference.attachment = 3;
			depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription& gbufferPass = subpasses[0];
			gbufferPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			gbufferPass.colorAttachmentCount = gbufferAttachmentReference.size();
			gbufferPass.pColorAttachments = gbufferAttachmentReference.data();
			gbufferPass.pDepthStencilAttachment = &depthAttachmentReference;
			gbufferPass.preserveAttachmentCount = 0;
			gbufferPass.pPreserveAttachments = nullptr;
			gbufferPass.pResolveAttachments = nullptr;

			deferredLightingPassInputAttachementReference[0].attachment = 0;
			deferredLightingPassInputAttachementReference[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			deferredLightingPassInputAttachementReference[1].attachment = 1;
			deferredLightingPassInputAttachementReference[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			deferredLightingPassInputAttachementReference[2].attachment = 2;
			deferredLightingPassInputAttachementReference[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			deferredLightingPassInputAttachementReference[3].attachment = 3;
			deferredLightingPassInputAttachementReference[3].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			deferredLightingPassOutputColorAttachmentReference[0].attachment = 4;
			deferredLightingPassOutputColorAttachmentReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription& deferredLighting = subpasses[1];
			deferredLighting.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			deferredLighting.inputAttachmentCount = deferredLightingPassInputAttachementReference.size();
			deferredLighting.pInputAttachments = deferredLightingPassInputAttachementReference.data();
			deferredLighting.colorAttachmentCount = deferredLightingPassOutputColorAttachmentReference.size();
			deferredLighting.pColorAttachments = deferredLightingPassOutputColorAttachmentReference.data();
			deferredLighting.pDepthStencilAttachment = nullptr;			// 以从之前的pass读取，不需要深度操作
			deferredLighting.preserveAttachmentCount = 0;
			deferredLighting.pPreserveAttachments = nullptr;

			// dependency
			VkSubpassDependency& deferredLightingDependOnShadowMapPass = dependencies[0];
			deferredLightingDependOnShadowMapPass.srcSubpass = VK_SUBPASS_EXTERNAL;
			deferredLightingDependOnShadowMapPass.dstSubpass = 1;
			deferredLightingDependOnShadowMapPass.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			deferredLightingDependOnShadowMapPass.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			deferredLightingDependOnShadowMapPass.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			deferredLightingDependOnShadowMapPass.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			deferredLightingDependOnShadowMapPass.dependencyFlags = 0;

			VkSubpassDependency& deferredLightingDependOnGbufferPass = dependencies[1];
			deferredLightingDependOnGbufferPass.srcSubpass = 0;
			deferredLightingDependOnGbufferPass.dstSubpass = 1;
			deferredLightingDependOnGbufferPass.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			deferredLightingDependOnGbufferPass.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			deferredLightingDependOnGbufferPass.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			deferredLightingDependOnGbufferPass.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;				// TODO

			VkSubpassDependency& presentDependOnDeferredLightingPass = dependencies[2];
			presentDependOnDeferredLightingPass.srcSubpass = 1;
			presentDependOnDeferredLightingPass.dstSubpass = VK_SUBPASS_EXTERNAL;
			presentDependOnDeferredLightingPass.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			presentDependOnDeferredLightingPass.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentDependOnDeferredLightingPass.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			presentDependOnDeferredLightingPass.dstAccessMask = 0;
		}

		VkRenderPassCreateInfo renderPassCI = {};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = attachments.size();
		renderPassCI.pAttachments = attachments.data();
		renderPassCI.subpassCount = subpasses.size();
		renderPassCI.pSubpasses = subpasses.data();
		renderPassCI.dependencyCount = dependencies.size();
		renderPassCI.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(vulkanRender->device, &renderPassCI, nullptr, &renderPass));
	}

	void DeferredRenderPass::setupFrameBuffers()
	{
		const std::vector<VkImageView>& imageViews = vulkanRender->swapChainImageViews;
		swapChainFrameBuffers.resize(imageViews.size());

		uint32_t swapChainWidth = vulkanRender->swapChainExtent.width;
		uint32_t swapChainHeight = vulkanRender->swapChainExtent.height;

		VkFormat format = vulkanRender->swapChainImageFormat;

		for (size_t i = 0; i < swapChainFrameBuffers.size(); i++)
		{
			std::array<VkImageView, 5> frameAttachments =
			{
				frameBuffers[0].attachments[0].imageView,
				frameBuffers[0].attachments[1].imageView,
				frameBuffers[0].attachments[2].imageView,
				vulkanRender->depthImageView,
				vulkanRender->swapChainImageViews[i]
			};

			VkFramebufferCreateInfo frameBufferCI = {};
			frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCI.flags = 0;
			frameBufferCI.renderPass = renderPass;
			frameBufferCI.attachmentCount = frameAttachments.size();
			frameBufferCI.pAttachments = frameAttachments.data();
			frameBufferCI.width = swapChainWidth;
			frameBufferCI.height = swapChainHeight;
			frameBufferCI.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(vulkanRender->device, &frameBufferCI, nullptr, &swapChainFrameBuffers[i]));
		}
	}

	void DeferredRenderPass::setupPipelines()
	{
		// gbuffer/lighting
		renderPipelines.resize(2);

		// gbuffer
		{
			std::array<VkDescriptorSetLayout, 2> descriptorSetLayout = { sceneData->uniformDescriptor.layout, sceneData->PBRMaterialDescriptor.layout };
			VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = descriptorSetLayout.size();
			pipelineLayoutCI.pSetLayouts = descriptorSetLayout.data();

			VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanRender->device, &pipelineLayoutCI, nullptr, &renderPipelines[0].layout));

			auto vertShaderCode = VulkanUtil::readFile(sceneData->shaderVSFliePath);
			auto fragShaderCode = VulkanUtil::readFile(sceneData->GBufferFSFilePath);
			
			auto bindingDescriptions = Vertex::getBindingDescriptions();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();

			std::vector<VkDynamicState> dynamicStates =
			{
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			};

			std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachmentState = {};
			colorBlendAttachmentState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState[0].blendEnable = VK_FALSE;

			colorBlendAttachmentState[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState[1].blendEnable = VK_FALSE;

			colorBlendAttachmentState[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState[2].blendEnable = VK_FALSE;

			VulkanPipeline::createPipeline(vulkanRender, renderPipelines[0].pipeline,
				renderPipelines[0].layout,
				vertShaderCode, fragShaderCode,
				bindingDescriptions, attributeDescriptions,
				renderPass,
				0,
				vulkanRender->viewport, vulkanRender->scissor,
				vulkanRender->msaaSamples,
				dynamicStates, 3, colorBlendAttachmentState.data(), true, true);
		}

		// lighting
		{
			std::array<VkDescriptorSetLayout, 3> descriptorSetLayout = { sceneData->directionalLightShadowDescriptor.layout, descriptorInfos[0].layout, sceneData->deferredUniformDescriptor.layout };
			VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = descriptorSetLayout.size();
			pipelineLayoutCI.pSetLayouts = descriptorSetLayout.data();

			VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanRender->device, &pipelineLayoutCI, nullptr, &renderPipelines[1].layout));

			auto vertShaderCode = VulkanUtil::readFile(sceneData->deferredLightingVSFilePath);
			auto fragShaderCode = VulkanUtil::readFile(sceneData->deferredLightingFSFilePath);

			std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
			std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;

			std::vector<VkDynamicState> dynamicStates;

			std::array<VkPipelineColorBlendAttachmentState, 1> colorBlendAttachmentState = {};
			colorBlendAttachmentState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState[0].blendEnable = VK_FALSE;

			VulkanPipeline::createPipeline(vulkanRender, renderPipelines[1].pipeline,
				renderPipelines[1].layout,
				vertShaderCode, fragShaderCode,
				vertexBindingDescriptions, vertexAttributeDescriptions,
				renderPass,
				1,
				vulkanRender->viewport, vulkanRender->scissor,
				vulkanRender->msaaSamples,
				dynamicStates, 1, colorBlendAttachmentState.data(), false, false);
		}
	}

	void DeferredRenderPass::setupDescriptorSetLayout()
	{
		std::array<VkDescriptorSetLayoutBinding, 4> gbufferDeferredLightingSetLayoutBinding = {};

		VkDescriptorSetLayoutBinding& gbufferNoramlSetLayoutBinding = gbufferDeferredLightingSetLayoutBinding[0];
		gbufferNoramlSetLayoutBinding.binding = 0;
		gbufferNoramlSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		gbufferNoramlSetLayoutBinding.descriptorCount = 1;
		gbufferNoramlSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding& gbufferMRSetLayoutBinding = gbufferDeferredLightingSetLayoutBinding[1];
		gbufferMRSetLayoutBinding.binding = 1;
		gbufferMRSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		gbufferMRSetLayoutBinding.descriptorCount = 1;
		gbufferMRSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding& gbufferAlbedoSetLayoutBinding = gbufferDeferredLightingSetLayoutBinding[2];
		gbufferAlbedoSetLayoutBinding.binding = 2;
		gbufferAlbedoSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		gbufferAlbedoSetLayoutBinding.descriptorCount = 1;
		gbufferAlbedoSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding& depthSetLayoutBinding = gbufferDeferredLightingSetLayoutBinding[3];
		depthSetLayoutBinding.binding = 3;
		depthSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthSetLayoutBinding.descriptorCount = 1;
		depthSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo gbufferDeferredLightingSetLayoutCI = {};
		gbufferDeferredLightingSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		gbufferDeferredLightingSetLayoutCI.pNext = nullptr;
		gbufferDeferredLightingSetLayoutCI.bindingCount = gbufferDeferredLightingSetLayoutBinding.size();
		gbufferDeferredLightingSetLayoutCI.pBindings = gbufferDeferredLightingSetLayoutBinding.data();

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRender->device, &gbufferDeferredLightingSetLayoutCI, nullptr, &descriptorInfos[0].layout));
	}

	void DeferredRenderPass::setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo gbufferLightDescriptorSetAllocateInfo = {};
		gbufferLightDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		gbufferLightDescriptorSetAllocateInfo.pNext = nullptr;
		gbufferLightDescriptorSetAllocateInfo.descriptorPool = vulkanRender->descriptorPool;
		gbufferLightDescriptorSetAllocateInfo.descriptorSetCount = 1;
		gbufferLightDescriptorSetAllocateInfo.pSetLayouts = &descriptorInfos[0].layout;
		
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRender->device, &gbufferLightDescriptorSetAllocateInfo, &descriptorInfos[0].descriptorSet));

		VkDescriptorImageInfo gbufferNormalInputAttachmentInfo = {};
		gbufferNormalInputAttachmentInfo.sampler = vulkanRender->getOrCreateNearestSampler();
		gbufferNormalInputAttachmentInfo.imageView = frameBuffers[0].attachments[0].imageView;
		gbufferNormalInputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo gbufferMRInputAttachmentInfo = {};
		gbufferMRInputAttachmentInfo.sampler = vulkanRender->getOrCreateNearestSampler();
		gbufferMRInputAttachmentInfo.imageView = frameBuffers[0].attachments[1].imageView;
		gbufferMRInputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo gbufferAlbedoInputAttachmentInfo = {};
		gbufferAlbedoInputAttachmentInfo.sampler = vulkanRender->getOrCreateNearestSampler();
		gbufferAlbedoInputAttachmentInfo.imageView = frameBuffers[0].attachments[2].imageView;
		gbufferAlbedoInputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo depthInputAttachmentInfo = {};
		depthInputAttachmentInfo.sampler = vulkanRender->getOrCreateNearestSampler();
		depthInputAttachmentInfo.imageView = vulkanRender->depthImageView;
		depthInputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<VkWriteDescriptorSet, 4> deferredLightingInputAttachmentWritesInfo = {};

		VkWriteDescriptorSet& gbufferNormalDescriptorInputAttachmentWriteInfo = deferredLightingInputAttachmentWritesInfo[0];
		gbufferNormalDescriptorInputAttachmentWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gbufferNormalDescriptorInputAttachmentWriteInfo.pNext = nullptr;
		gbufferNormalDescriptorInputAttachmentWriteInfo.dstSet = descriptorInfos[0].descriptorSet;
		gbufferNormalDescriptorInputAttachmentWriteInfo.dstBinding = 0;
		gbufferNormalDescriptorInputAttachmentWriteInfo.dstArrayElement = 0;
		gbufferNormalDescriptorInputAttachmentWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		gbufferNormalDescriptorInputAttachmentWriteInfo.descriptorCount = 1;
		gbufferNormalDescriptorInputAttachmentWriteInfo.pImageInfo = &gbufferNormalInputAttachmentInfo;

		VkWriteDescriptorSet& gbufferMRDescriptorInputAttachmentWriteInfo = deferredLightingInputAttachmentWritesInfo[1];
		gbufferMRDescriptorInputAttachmentWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gbufferMRDescriptorInputAttachmentWriteInfo.pNext = nullptr;
		gbufferMRDescriptorInputAttachmentWriteInfo.dstSet = descriptorInfos[0].descriptorSet;
		gbufferMRDescriptorInputAttachmentWriteInfo.dstBinding = 1;
		gbufferMRDescriptorInputAttachmentWriteInfo.dstArrayElement = 0;
		gbufferMRDescriptorInputAttachmentWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		gbufferMRDescriptorInputAttachmentWriteInfo.descriptorCount = 1;
		gbufferMRDescriptorInputAttachmentWriteInfo.pImageInfo = &gbufferMRInputAttachmentInfo;

		VkWriteDescriptorSet& gbufferAlbedoDescriptorInputAttachmentWriteInfo = deferredLightingInputAttachmentWritesInfo[2];
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.pNext = nullptr;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.dstSet = descriptorInfos[0].descriptorSet;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.dstBinding = 2;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.dstArrayElement = 0;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.descriptorCount = 1;
		gbufferAlbedoDescriptorInputAttachmentWriteInfo.pImageInfo = &gbufferAlbedoInputAttachmentInfo;

		VkWriteDescriptorSet& depthDescriptorInputAttachmentWriteInfo = deferredLightingInputAttachmentWritesInfo[3];
		depthDescriptorInputAttachmentWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depthDescriptorInputAttachmentWriteInfo.pNext = nullptr;
		depthDescriptorInputAttachmentWriteInfo.dstSet = descriptorInfos[0].descriptorSet;
		depthDescriptorInputAttachmentWriteInfo.dstBinding = 3;
		depthDescriptorInputAttachmentWriteInfo.dstArrayElement = 0;
		depthDescriptorInputAttachmentWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthDescriptorInputAttachmentWriteInfo.descriptorCount = 1;
		depthDescriptorInputAttachmentWriteInfo.pImageInfo = &depthInputAttachmentInfo;

		vkUpdateDescriptorSets(vulkanRender->device, deferredLightingInputAttachmentWritesInfo.size(), deferredLightingInputAttachmentWritesInfo.data(), 0, nullptr);
	}

}