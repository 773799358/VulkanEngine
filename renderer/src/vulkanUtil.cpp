#include "vulkanUtil.hpp"
#include <fstream>
#include <macro.hpp>

namespace VulkanEngine
{
	std::vector<char> vulkanUtil::readFile(const std::string& filename)
	{
		// ate:从文件末尾开始读取，能根据位置来确定文件大小
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			LOG_ERROR("failed to open file");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		// 回到开头
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}
}