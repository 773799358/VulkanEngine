#include "UIPass.hpp"
#include "macro.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_sdl2.h"

namespace VulkanEngine
{

	void UIPass::init(VulkanRenderer* vulkanRender, VulkanRenderPass* mainPass)
	{
		VulkanRenderPass::init(vulkanRender);

		ImGui::CreateContext();
		ImGui_ImplSDL2_InitForVulkan(vulkanRender->window);

		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance = vulkanRender->instance;
		initInfo.PhysicalDevice = vulkanRender->physicalDevice;
		initInfo.Device = vulkanRender->device;
		initInfo.QueueFamily = vulkanRender->queueIndices.graphicsFamily.value();
		initInfo.Queue = vulkanRender->graphicsQueue;
		initInfo.DescriptorPool = vulkanRender->descriptorPool;

		initInfo.MinImageCount = vulkanRender->MAX_FRAMES_IN_FLIGHT;
		initInfo.ImageCount = vulkanRender->MAX_FRAMES_IN_FLIGHT;

		initInfo.RenderPass = mainPass->renderPass;
		ImGui_ImplVulkan_Init(&initInfo);

		uploadFonts();
	}

	void UIPass::postInit()
	{
	}

	void UIPass::uploadFonts()
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = vulkanRender->mainCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		if (VK_SUCCESS != vkAllocateCommandBuffers(vulkanRender->device, &allocInfo, &commandBuffer))
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer, &beginInfo))
		{
			throw std::runtime_error("Could not create one-time command buffer!");
		}

		if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer))
		{
			throw std::runtime_error("failed to record command buffer!");
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(vulkanRender->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(vulkanRender->graphicsQueue);

		vkFreeCommandBuffers(vulkanRender->device, vulkanRender->mainCommandPool, 1, &commandBuffer);
	}

	void UIPass::draw(VkCommandBuffer commandBuffer)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vulkanRender->getCurrentCommandBuffer());
	}

	void UIPass::clear()
	{
		ImGui_ImplVulkan_Shutdown();
	}
}