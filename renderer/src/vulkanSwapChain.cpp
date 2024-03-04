#include "macro.hpp"
#include "vulkanSwapChain.hpp"
#include "vulkanUtil.hpp"

namespace VulkanEngine
{
	void VulkanSwapChain::setSurface(VkSurfaceKHR surface)
	{
		this->surface = surface;
	}

	void VulkanSwapChain::connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		this->instance = instance;
		this->physicalDevice = physicalDevice;
		this->device = device;
	}

	void VulkanSwapChain::create(uint32_t* width, uint32_t* height, bool vsync, bool fullscreen)
	{
		// 用来简化重新创建
		VkSwapchainKHR oldSwapchain = swapChain;

		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

		// 如果没垂直同步，就使用邮箱模式或者即时呈现模式
		if (!vsync)
		{
			for (size_t i = 0; i < swapChainSupport.presentModes.size(); i++)
			{
				if (swapChainSupport.presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if (swapChainSupport.presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		}

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		// 如果不支持三缓冲或者双缓冲，就用最大能支持的
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}
		LOG_INFO("swapChain image count: {} ", imageCount);

		// 不需要对图像进行旋转或者翻转操作
		VkSurfaceTransformFlagsKHR preTransform;
		if (swapChainSupport.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else
		{
			preTransform = swapChainSupport.capabilities.currentTransform;
		}

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		info.imageExtent = chooseSwapExtent(swapChainSupport.capabilities, width, height);
		*width = info.imageExtent.width;
		*height = info.imageExtent.height;

		info.colorFormat = surfaceFormat.format;
		info.colorSpace = surfaceFormat.colorSpace;

		// 不会图像进行混合
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		VkSwapchainCreateInfoKHR swapchainCI = {};
		swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCI.surface = surface;
		swapchainCI.minImageCount = imageCount;
		swapchainCI.imageFormat = info.colorFormat;
		swapchainCI.imageColorSpace = info.colorSpace;
		swapchainCI.imageExtent = { info.imageExtent.width, info.imageExtent.height };
		swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
		swapchainCI.imageArrayLayers = 1;
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.presentMode = swapchainPresentMode;

		// 尽量支持更多种方式
		if (swapChainSupport.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		if (swapChainSupport.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

		// 渲染和呈现队列是否是一个
		if (indices.graphicsFamily != indices.presentFamily)
		{
			swapchainCI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCI.queueFamilyIndexCount = 2;
			swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			// 这两个参数是用来进行image在多个队列共享的，同一个队列就设置为空
			swapchainCI.queueFamilyIndexCount = 0;
			swapchainCI.pQueueFamilyIndices = nullptr;
		}

		VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapChain));

		// 如果重新创建进行资源清理
		if (oldSwapchain != VK_NULL_HANDLE)
		{
			for (uint32_t i = 0; i < imageCount; i++)
			{
				vkDestroyImageView(device, images[i].view, nullptr);
			}
			vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
		}
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr));

		std::vector<VkImage>tempImages(imageCount);
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, tempImages.data()));

		images.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++)
		{
			VkImageViewCreateInfo colorAttachmentView = {};
			colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorAttachmentView.pNext = nullptr;
			colorAttachmentView.format = info.colorFormat;
			colorAttachmentView.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			};
			colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorAttachmentView.subresourceRange.baseMipLevel = 0;
			colorAttachmentView.subresourceRange.levelCount = 1;
			colorAttachmentView.subresourceRange.baseArrayLayer = 0;
			colorAttachmentView.subresourceRange.layerCount = 1;
			colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorAttachmentView.flags = 0;

			images[i].image = tempImages[i];

			colorAttachmentView.image = images[i].image;

			VK_CHECK_RESULT(vkCreateImageView(device, &colorAttachmentView, nullptr, &images[i].view));
		}
	}

	VkResult VulkanSwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
	{
		return vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
	}

	VkResult VulkanSwapChain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
	{
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = NULL;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;

		// 检查在显示图像之前是否指定了等待信号量
		if (waitSemaphore != VK_NULL_HANDLE)
		{
			presentInfo.pWaitSemaphores = &waitSemaphore;
			presentInfo.waitSemaphoreCount = 1;
		}
		return vkQueuePresentKHR(queue, &presentInfo);
	}

	void VulkanSwapChain::cleanup()
	{
		if (swapChain != VK_NULL_HANDLE)
		{
			for (uint32_t i = 0; i < images.size(); i++)
			{
				vkDestroyImageView(device, images[i].view, nullptr);
			}
		}
		if (surface != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, swapChain, nullptr);
			vkDestroySurfaceKHR(instance, surface, nullptr);
		}
		surface = VK_NULL_HANDLE;
		swapChain = VK_NULL_HANDLE;
	}

	VulkanSwapChain::SwapChainSupportDetails VulkanSwapChain::querySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;
		// 查询支持哪些功能
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		// 查询格式
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		// 查询呈现模式
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR VulkanSwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		VkSurfaceFormatKHR format;
		// 每个VkSurfaceFormatKHR结构都包含一个format和一个colorSpace
		// format指定颜色通道和类型

		// 如果只有一种格式，就用RGBA
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			format.format = VK_FORMAT_B8G8R8A8_UNORM;
			format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

			return format;
		}

		// 可以选的话
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		// 如果都失效了，就只能选第一个了
		return availableFormats[0];
	}

	VkExtent2D VulkanSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t* width, uint32_t* height)
	{
		// 让swapChain的图像大小尽量等于窗体大小
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { *width, *height };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

}