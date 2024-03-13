#include "renderPass_directionalLightShadow.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"
#include "vulkanPipeline.hpp"

namespace VulkanEngine
{
	void DirectionalLightShadowMapRenderPass::init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData)
	{
		VulkanRenderPass::init(vulkanRender, sceneData);

		descriptorInfos.resize(1);
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

		vkDestroyDescriptorSetLayout(vulkanRender->device, descriptorInfos[0].layout, nullptr);
		vkFreeDescriptorSets(vulkanRender->device, vulkanRender->descriptorPool, 1, &descriptorInfos[0].descriptorSet);

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

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanRender->device, &layoutCI, nullptr, &descriptorInfos[0].layout));
	}

	void DirectionalLightShadowMapRenderPass::setupPipelines()
	{
		renderPipelines.resize(1);

		// layout
		VkDescriptorSetLayout descriptorSetLayouts[] = { descriptorInfos[0].layout };

		VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
		pipelineLayoutCI.pSetLayouts = descriptorSetLayouts;

		VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanRender->device, &pipelineLayoutCI, nullptr, &renderPipelines[0].layout));

		// 配置一次subpass的状态，DX内对应PipelineStateObject
		auto vertShaderCode = VulkanUtil::readFile(sceneData->shadowVSFilePath);
		auto fragShaderCode = VulkanUtil::readFile(sceneData->shadowFSFilePath);

		// 顶点数据描述
		auto vertexBindingDescriptions = Vertex::getBindingDescriptions();
		auto vertexAttributeDescription = Vertex::getAttributeDescriptions()[0];
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions = { vertexAttributeDescription };

		// 视口与裁剪
		VkViewport viewport = { 0, 0, frameBuffers[0].width, frameBuffers[0].height, 0.0, 1.0 };
		VkRect2D scissor = { {0, 0}, { frameBuffers[0].width, frameBuffers[0].height } };

		std::vector<VkDynamicState> dynamicStates;

		std::array<VkPipelineColorBlendAttachmentState, 1> colorBlendAttachmentState = {};
		colorBlendAttachmentState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentState[0].blendEnable = VK_FALSE;

		VulkanPipeline::createPipeline(vulkanRender, renderPipelines[0].pipeline,
			renderPipelines[0].layout,
			vertShaderCode, fragShaderCode,
			vertexBindingDescriptions, vertexAttributeDescriptions,
			renderPass,
			0,
			vulkanRender->viewport, vulkanRender->scissor,
			vulkanRender->msaaSamples,
			dynamicStates, 1, colorBlendAttachmentState.data(), true, true);
	}

	void DirectionalLightShadowMapRenderPass::setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = vulkanRender->descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorInfos[0].layout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanRender->device, &descriptorSetAllocateInfo, &descriptorInfos[0].descriptorSet));

		VkDescriptorBufferInfo uniformBufferInfo[2] = {};
		uniformBufferInfo[0].offset = 0;
		uniformBufferInfo[0].buffer = sceneData->uniformShadowResource.buffer;
		uniformBufferInfo[0].range = sizeof(UnifromBufferObjectShadowProjView);

		uniformBufferInfo[1].offset = 0;
		uniformBufferInfo[1].buffer = sceneData->uniformDynamicResource.buffer;
		uniformBufferInfo[1].range = sizeof(UniformBufferDynamicObject);
		
		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].pNext = nullptr;
		descriptorWrites[0].dstSet = descriptorInfos[0].descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &uniformBufferInfo[0];

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].pNext = nullptr;
		descriptorWrites[1].dstSet = descriptorInfos[0].descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &uniformBufferInfo[1];

		vkUpdateDescriptorSets(vulkanRender->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

}