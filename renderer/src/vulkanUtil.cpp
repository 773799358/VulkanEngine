﻿#include "vulkanUtil.hpp"
#include <fstream>
#include <macro.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace VulkanEngine
{
	bool createDirectoryIfNotExists(const fs::path& path) 
	{
		if (!fs::exists(path)) 
		{
			try {
				fs::create_directories(path);
				return true;
			}
			catch (const fs::filesystem_error& e) {
				LOG_ERROR("failed to create directory: {}", e.what());
				return false;
			}
		}
		else 
		{
			return true; // 目录已经存在，返回true表示操作成功  
		}
	}

	std::vector<char> VulkanUtil::readFile(const std::string& filename)
	{
		// ate:从文件末尾开始读取，能根据位置来确定文件大小
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			LOG_ERROR("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		// 回到开头
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	void VulkanUtil::saveFile(const std::string& filename, const std::vector<unsigned char>& data)
	{
		std::string dir = filename.substr(0, filename.find_last_of('/'));
		createDirectoryIfNotExists(dir);

		std::ofstream file(filename, std::ios::out | std::ios::binary);

		if (file.is_open())
		{
			file.write(reinterpret_cast<const char*>(&data[0]), data.size());
			file.close();
		}
		else
		{
			LOG_ERROR("failed to open file!");
		}
	}

	VkSampleCountFlagBits VulkanUtil::getMaxUsableSampleCount(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = std::min(physicalDeviceProperties.limits.framebufferColorSampleCounts, physicalDeviceProperties.limits.framebufferDepthSampleCounts);
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

}