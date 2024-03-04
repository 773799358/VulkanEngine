#pragma once

#include "vulkan/vulkan.h"

namespace VulkanEngine
{
    // 尽量做到资源复用
    class vulkanResource
    {
    public:
        static void createImage(
            VkPhysicalDevice physicalDevice, 
            VkDevice device,
            uint32_t imageWidth,
            uint32_t imageHeight,
            VkFormat format,
            VkImageTiling imageTiling,
            VkImageUsageFlags imageUsageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkImage& image,
            VkDeviceMemory& memory,
            VkImageCreateFlags imageCreateFlags,
            uint32_t arrayLayers,
            uint32_t miplevels);

        static VkImageView createImageView(
            VkDevice device,
            VkImage& image,
            VkFormat format,
            VkImageAspectFlags imageAspectFlags,
            VkImageViewType viewType,
            uint32_t layoutCount,
            uint32_t miplevels);
    };
}