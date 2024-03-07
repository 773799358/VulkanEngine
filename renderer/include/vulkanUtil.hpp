#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace VulkanEngine
{
	class VulkanUtil
	{
	public:
		static std::vector<char> readFile(const std::string& filename);
		static VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice);
	};
}