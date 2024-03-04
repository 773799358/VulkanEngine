#include "macro.hpp"
#include "SDL2/SDL_vulkan.h"
#include "renderer.hpp"
#include "vulkanDebug.hpp"
#include "vulkanUtil.hpp"

#include <iostream>
#include <map>
#include <set>

namespace VulkanEngine
{
	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	Renderer::Renderer(const std::string& basePash)
	{
		this->basePath = basePash + "/";
	}

	Renderer::~Renderer()
	{
		clear();
	}

	void Renderer::init()
	{
		initWindow();
		initVulkan();
	}

	void Renderer::run()
	{
		bool shouldClose = false;
		SDL_Event event;
		bool pauseRender = false;
		while (!shouldClose)
		{
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_QUIT)
				{
					shouldClose = true;
				}

				if (event.type == SDL_WINDOWEVENT)
				{
					if (event.window.event == SDL_WINDOWEVENT_RESIZED)
					{
						int newW = event.window.data1;
						int newH = event.window.data2;
						onWindowResized(window, newW, newH);
					}
					if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
					{
						pauseRender = true;
					}
					if (event.window.event == SDL_WINDOWEVENT_RESTORED)
					{
						int newW = event.window.data1;
						int newH = event.window.data2;
						pauseRender = false;
						onWindowResized(window, newW, newH);
					}
				}
			}

			if (!pauseRender)
			{
				drawFrame();
			}
		}
	}

	void Renderer::initWindow()
	{
		SDL_Init(SDL_INIT_EVERYTHING);

		window = SDL_CreateWindow("Vulkan Engine",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height,
			SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	}

	void Renderer::onWindowResized(SDL_Window* window, int width, int height)
	{
	}

	void Renderer::initVulkan()
	{
		createInstance();
		createSurface();
		pickPhysicalDevice();
		createLogicDevice();
		createSwapChain();
	}

	bool Renderer::checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	std::vector<const char*> Renderer::getRequiredExtensions()
	{
		// window扩展
		uint32_t count;
		SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
		std::vector<const char*> extensions(count);
		SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());

		LOG_INFO("surface extensions:");
		for (auto& extension : extensions)
		{
			// 两个扩展，一个是surface扩展，一个是window平台的win32 surface扩展
			LOG_INFO("\t {}", extension);
		}

		// 开启验证层
		if (enableValidationLayers)
		{
			// debug util ext
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void Renderer::createInstance()
	{
		// 查看扩展支持
		{
			uint32_t extensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
			std::vector<VkExtensionProperties> extensions(extensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

			LOG_INFO("available extensions:");

			for (const auto& extension : extensions)
			{
				supportedInstanceExtensions.push_back(extension.extensionName);
				LOG_INFO("\t {}", extension.extensionName);
			}
		}

		// 检查验证层扩展
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			LOG_ERROR("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;				// 用于指定扩展
		appInfo.pApplicationName = "Vulkan Engine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();

		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

		VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

		if(enableValidationLayers)
			debug::setupDebugging(instance);
	}

	void Renderer::createSurface()
	{
		if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
		{
			LOG_ERROR("can't create surface");
		}
	}

	// 获取一张性能最好的显卡，如果没有独显，就使用核显
	int rateDeviceSuitability(VkPhysicalDevice device)
	{
		// GPU基础属性，例如name、type、vulkanVersion等
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// GPU支持的功能，例如纹理压缩、64为浮点数、多视图渲染（VR）等
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		int score = 0;

		// 支持geometry
		if (!deviceFeatures.geometryShader)
		{
			return 0;
		}

		// 优先独显
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		// 找一张支持纹理最大的显卡
		score += deviceProperties.limits.maxImageDimension2D;

		LOG_INFO("\t {}", deviceProperties.deviceName);

		return score;
	}

	bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device)
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

	bool Renderer::isDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices = findQueueFamilies(device, surface);

		// 检查扩展支持
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		// 检查对swapChain的支持
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			// 至少得支持一种格式和一种呈现模式
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		// 检查支持的特性
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.isComplete() && extensionsSupported && supportedFeatures.samplerAnisotropy && swapChainAdequate;
	}

	void Renderer::createLogicDevice()
	{
		vulkanDevice.reset(new VulkanDevice(physicalDevice));

		// 除了各向异性，暂时不需要扩展什么功能，默认值一切feature都为false
		VkPhysicalDeviceFeatures enabledFeatures = {};
		enabledFeatures.samplerAnisotropy = VK_TRUE;

		// 这里如果crash，一定去校验扩展设置
		VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, deviceExtensions, nullptr);
		if (res != VK_SUCCESS) 
		{
			LOG_ERROR("Could not create Vulkan device!");
		}
	}

	void Renderer::createSwapChain()
	{
		vulkanSwapChain.reset(new VulkanSwapChain());

		vulkanSwapChain->setSurface(surface);
		vulkanSwapChain->connect(instance, physicalDevice, vulkanDevice->logicalDevice);
		vulkanSwapChain->create(&width, &height);
	}

	void Renderer::pickPhysicalDevice()
	{
		// 显卡列表
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
		if (physicalDeviceCount == 0)
		{
			LOG_ERROR("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, devices.data());

		// 对显卡性能进行排序
		std::multimap<int, VkPhysicalDevice> candidates;
		LOG_INFO("GPUs:");
		for (const auto& device : devices)
		{
			int score = rateDeviceSuitability(device);
			if (isDeviceSuitable(device))
			{
				candidates.insert(std::make_pair(score, device));
			}
		}

		// 选择数值最大的显卡
		if (candidates.rbegin()->first > 0)
		{
			physicalDevice = candidates.rbegin()->second;
		}
		else
		{
			LOG_ERROR("failed to find a suitable GPU!");
		}
	}

	void Renderer::drawFrame()
	{
	}

	void Renderer::clear()
	{
		vulkanSwapChain.reset();
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vulkanDevice.reset();
		vkDestroyInstance(instance, nullptr);

		SDL_DestroyWindow(window);
		SDL_Quit();
	}

}