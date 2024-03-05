#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace VulkanEngine
{
	class vulkanUtil
	{
	public:
		static std::vector<char> readFile(const std::string& filename);
	};
}