#pragma once

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include <vector>

namespace VulkanEngine
{
	// 注意16字节对齐

	struct VulkanResource
	{
		VkBuffer* buffer;
		VkDeviceMemory memory;
	};

	struct ShaderData
	{
		glm::mat4 view;
		glm::mat4 proj;

		VulkanResource resource;
	};
}