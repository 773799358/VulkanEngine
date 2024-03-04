#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanEngine
{
    struct QueueFamilyIndices
    {
        int graphicsFamily = -1;
        int presentFamily = -1;
        bool isComplete();
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
}