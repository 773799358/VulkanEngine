#pragma once

#include "vulkan/vulkan.h"
#include <vector>

namespace VulkanEngine
{
	struct SwapChainImage
	{
		VkImage image;
		VkImageView view;
	};

	struct SwapChainInfo
	{
		VkExtent2D imageExtent;
		uint32_t imageCount;
		VkFormat colorFormat;
		VkColorSpaceKHR colorSpace;
		uint32_t queueNodeIndex = UINT32_MAX;
	};

	class VulkanSwapChain
	{
	private:
		VkInstance instance;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkSurfaceKHR surface;

		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		VulkanSwapChain::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t* width, uint32_t* height);

	public:
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		std::vector<SwapChainImage> images;
		SwapChainInfo info;

		void setSurface(VkSurfaceKHR surface);
		void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
		void create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen = false);
		VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
		VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
		void cleanup();
	};
}