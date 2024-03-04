#pragma once

#include "vulkan/vulkan.h"
#include "logger.hpp"
#include "errorString.hpp"
#include <string>
#include <iostream>

#define LOG_HELPER(LOG_LEVEL, ...) \
    globalLogger.log(LOG_LEVEL, "[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_DEBUG(...) LOG_HELPER(LogSystem::LogLevel::debug, __VA_ARGS__);

#define LOG_INFO(...) LOG_HELPER(LogSystem::LogLevel::info, __VA_ARGS__);

#define LOG_WARN(...) LOG_HELPER(LogSystem::LogLevel::warn, __VA_ARGS__);

#define LOG_ERROR(...) LOG_HELPER(LogSystem::LogLevel::error, __VA_ARGS__);

#define LOG_FATAL(...) LOG_HELPER(LogSystem::LogLevel::fatal, __VA_ARGS__);

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		LOG_FATAL("Fatal : VkResult is {}", errorString(res));                                          \
		assert(res == VK_SUCCESS);																		\
	}																									\
}