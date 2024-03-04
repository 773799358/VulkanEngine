#include "vulkanRenderer.hpp"
#include "macro.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#include "vulkanResource.hpp"

#include <algorithm>
#include <set>

namespace VulkanEngine
{
    // 非debug不开启验证层
#ifdef NDEBUG
    const bool debug = false;
#else
    const bool debug = true;
#endif // NDEBUG

    const std::vector<const char*> validationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // 捕获验证层的message
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }

    VulkanRenderer::~VulkanRenderer()
    {
        clearSwapChain();

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, imageAvailableForRenderSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageFinishedForPresentSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableForTextureCopySemaphores[i], nullptr);

            vkDestroyFence(device, isFrameInFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, mainCommandPool, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyCommandPool(device, commandPools[i], nullptr);
        }

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    void VulkanRenderer::init(Window* window)
    {
        windowHandler = window;
        this->window = window->getWindow();

        windowWidth = window->getWindowSize()[0];
        windowHeight = window->getWindowSize()[1];

        viewport = { 0.0f, 0.0f, (float)windowWidth, (float)windowHeight, 0.0f, 1.0f };
        scissor = { {0, 0}, { windowWidth, windowHeight} };

        if (!debug)
        {
            enableValidationLayers = false;
            enableDebugUtilsLabel = false;
        }

        createInstance();

        setupDebugMessenger();

        createWindowSurface();

        pickPhysicalDevice();

        createLogicalDevice();

        createCommandPool();

        createCommandBuffers();

        createDescriptorPool();

        createSyncPrimitives();

        createSwapchain();

        createSwapchainImageViews();

        createFramebufferImageAndView();
    }

    void VulkanRenderer::createInstance()
    {
        // 不支持验证层
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            LOG_ERROR("validation layers requested, but not available!");
        }

        // app into
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = vulkanAPIVersion;

        // create instance
        VkInstanceCreateInfo instanceCI = {};
        instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCI.pApplicationInfo = &appInfo; // the appInfo is stored here

        auto extensions = getRequiredExtensions();
        instanceCI.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCI.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
        if (enableValidationLayers)
        {
            instanceCI.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCI.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            instanceCI.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else
        {
            instanceCI.enabledLayerCount = 0;
            instanceCI.pNext = nullptr;
        }

        VK_CHECK_RESULT(vkCreateInstance(&instanceCI, nullptr, &instance));
    }

    void VulkanRenderer::setupDebugMessenger()
    {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        populateDebugMessengerCreateInfo(createInfo);

        VK_CHECK_RESULT(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger));
    }

    void VulkanRenderer::createWindowSurface()
    {
        if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
        {
            LOG_ERROR("can't create surface");
        }
    }

    void VulkanRenderer::pickPhysicalDevice()
    {
        uint32_t physicalDeviceCount;
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
        if (physicalDeviceCount == 0)
        {
            LOG_ERROR("enumerate physical devices failed!");
        }
        else
        {
            // 对显卡性能进行排序，挑个最好的
            std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
            vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

            std::vector<std::pair<int, VkPhysicalDevice>> rankedPhysicalDevices;
            for (const auto& device : physicalDevices)
            {
                VkPhysicalDeviceProperties physicalDeviceProperties;
                vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);
                int score = 0;

                if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    score += 1000;
                }
                else if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                {
                    score += 100;
                }

                rankedPhysicalDevices.push_back({ score, device });
            }

            std::sort(rankedPhysicalDevices.begin(),
                rankedPhysicalDevices.end(),
                [](const std::pair<int, VkPhysicalDevice>& p1, const std::pair<int, VkPhysicalDevice>& p2) {
                    return p1 > p2;
                });

            for (const auto& device : rankedPhysicalDevices)
            {
                if (isDeviceSuitable(device.second))
                {
                    physicalDevice = device.second;
                    break;
                }
            }

            if (physicalDevice == VK_NULL_HANDLE)
            {
                LOG_ERROR("failed to find suitable physical device");
            }
        }
    }

    void VulkanRenderer::createLogicalDevice()
    {
        queueIndices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCIs; // all queues that need to be created
        std::set<uint32_t> queueFamilies = { queueIndices.graphicsFamily.value(),
                                             queueIndices.presentFamily.value(),
                                             queueIndices.computeFamily.value() };

        float queue_priority = 1.0f;
        for (uint32_t queueFamily : queueFamilies) // for every queue family
        {
            // queue create info
            VkDeviceQueueCreateInfo queueCI{};
            queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCI.queueFamilyIndex = queueFamily;
            queueCI.queueCount = 1;
            queueCI.pQueuePriorities = &queue_priority;
            queueCIs.push_back(queueCI);
        }

        // physical device features
        VkPhysicalDeviceFeatures physicalDeviceFeatures = {};

        // 支持各向异性
        physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

        // 支持低效率的回读存储缓冲区
        physicalDeviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

        // 支持独立混合
        physicalDeviceFeatures.independentBlend = VK_TRUE;

        // 支持geometry shader
        physicalDeviceFeatures.geometryShader = VK_TRUE;
        
        // deviceCI
        VkDeviceCreateInfo deviceCI{};
        deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCI.pQueueCreateInfos = queueCIs.data();
        deviceCI.queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size());
        deviceCI.pEnabledFeatures = &physicalDeviceFeatures;
        deviceCI.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCI.ppEnabledExtensionNames = deviceExtensions.data();
        deviceCI.enabledLayerCount = 0;

        VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device));

        // 初始化队列
        vkGetDeviceQueue(device, queueIndices.graphicsFamily.value(), 0, &graphicsQueue);

        vkGetDeviceQueue(device, queueIndices.presentFamily.value(), 0, &presentQueue);

        vkGetDeviceQueue(device, queueIndices.computeFamily.value(), 0, &computeQueue);

        // 查询深度支持的格式
        depthImageFormat = findDepthFormat();
    }

    void VulkanRenderer::createCommandPool()
    {
        // TODO:不清楚多个command pool会对性能产生怎样的影响
        {
            VkCommandPoolCreateInfo commandPoolCI{};
            commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolCI.pNext = nullptr;
            commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            commandPoolCI.queueFamilyIndex = queueIndices.graphicsFamily.value();

            VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCI, nullptr, &mainCommandPool));
        }

        // other command pools
        {
            VkCommandPoolCreateInfo commandPoolCI;
            commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolCI.pNext = nullptr;
            commandPoolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            commandPoolCI.queueFamilyIndex = queueIndices.graphicsFamily.value();

            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPools[i]));
            }
        }
    }

    void VulkanRenderer::createCommandBuffers()
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;      // 主commandBuffer
        commandBufferAllocateInfo.commandBufferCount = 1U;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            commandBufferAllocateInfo.commandPool = commandPools[i];
            VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffers[i]));
        }
    }

    void VulkanRenderer::createDescriptorPool()
    {
        // TODO:DescriptorSet作为一种资源，应该想内存那样进行复用管理，不过暂时开足够大，以免不够用
        VkDescriptorPoolSize poolSizes[7];
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC; // 多pass
        poolSizes[0].descriptorCount = 3 + 2 + 2 + 2 + 1 + 1 + 3 + 3;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;         // 粒子系统、骨骼动画
        poolSizes[1].descriptorCount = 1 + 1 + 1 * maxVertexBlendingMeshCount;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[2].descriptorCount = 1 * maxMaterialCount;
        poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[3].descriptorCount = 3 + 5 * maxMaterialCount + 1 + 1; // ImGui_ImplVulkan_CreateDeviceObjects
        poolSizes[4].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        poolSizes[4].descriptorCount = 4 + 1 + 1 + 2;
        poolSizes[5].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        poolSizes[5].descriptorCount = 3;
        poolSizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[6].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets =
            1 + 1 + 1 + maxMaterialCount + maxVertexBlendingMeshCount + 1 + 1; // +skybox + axis descriptor set
        poolInfo.flags = 0U;

        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
    }

    void VulkanRenderer::createSyncPrimitives()
    {
        // 主要是渲染和呈现之间的同步
        VkSemaphoreCreateInfo semaphoreCI{};
        semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态要为true，否则会导致一直等待

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableForRenderSemaphores[i]));
            VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageFinishedForPresentSemaphores[i]));
            VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableForTextureCopySemaphores[i]));
            VK_CHECK_RESULT(vkCreateFence(device, &fenceCI, nullptr, &isFrameInFlightFences[i]));
        }
    }

    void VulkanRenderer::createSwapchain()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // 如果不支持三缓冲或者双缓冲，就用最大能支持的
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.presentMode = presentMode;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;	// 2D图像
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	// 也可以首先将图像渲染为单独的图像，进行后处理操作。在这种情况下可以使用像VK_IMAGE_USAGE_TRANSFER_DST_BIT这样的值，并使用内存操作将渲染的图像传输到交换链图像队列。

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { queueIndices.graphicsFamily.value(), indices.presentFamily.value() };

        // 渲染和呈现队列是否是一个
        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // 这两个参数是用来进行image在多个队列共享的，同一个队列就设置为空
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;	// 这里不需要进行图像的旋转或者翻转操作
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;				// 也不需要与其他窗体进行混合
        createInfo.clipped = VK_TRUE;												// 如果有像素被其他窗体遮挡，可以进行优化，不对图像进行回读，所以不关心
        createInfo.oldSwapchain = VK_NULL_HANDLE;									// swapChain在窗口调整大小时需要被替换，这里不考虑，指定oldSwapChain用来进行回收

        VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain));

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        scissor = { {0, 0}, {swapChainExtent.width, swapChainExtent.height} };
    }

    void VulkanRenderer::clearSwapChain()
    {
        for (size_t i = 0; i < swapChainImageViews.size(); i++)
        {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void VulkanRenderer::recreateSwapchain()
    {
        windowWidth = windowHandler->getWindowSize()[0];
        windowWidth = windowHandler->getWindowSize()[1];

        // TODO:这样判断最小化安全么？
        while (windowWidth == 0 || windowWidth == 0)
        {
            windowWidth = windowHandler->getWindowSize()[0];
            windowWidth = windowHandler->getWindowSize()[1];
        }

        VkResult resWaitForFences = vkWaitForFences(device, MAX_FRAMES_IN_FLIGHT, isFrameInFlightFences, VK_TRUE, UINT64_MAX);
        if (VK_SUCCESS != resWaitForFences)
        {
            LOG_ERROR("WaitForFences failed");
            return;
        }

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        for (auto imageview : swapChainImageViews)
        {
            vkDestroyImageView(device, imageview, nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        createSwapchain();
        createSwapchainImageViews();
        createFramebufferImageAndView();
    }

    void VulkanRenderer::createSwapchainImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            swapChainImageViews[i] = vulkanResource::createImageView(
                device, 
                swapChainImages[i], 
                swapChainImageFormat, 
                VK_IMAGE_ASPECT_COLOR_BIT, 
                VK_IMAGE_VIEW_TYPE_2D, 
                1, 
                1);
        }
    }

    void VulkanRenderer::createFramebufferImageAndView()
    {
        vulkanResource::createImage(
            physicalDevice,
            device,
            swapChainExtent.width,
            swapChainExtent.height,
            depthImageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImage,
            depthImageMemory,
            0,
            1,
            1);

        depthImageView = vulkanResource::createImageView(
            device,
            depthImage,
            depthImageFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            1,
            1);
    }

    bool VulkanRenderer::checkValidationLayerSupport()
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

    std::vector<const char*> VulkanRenderer::getRequiredExtensions()
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

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        // 输出到控制台
        std::cerr << "validation layer: " << "\n" << "\t" << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice physicalDevice)
    {
        auto indices = findQueueFamilies(physicalDevice);

        // 检查扩展支持
        bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);

        // 检查对swapChain的支持
        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            // 至少得支持一种格式和一种呈现模式
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        // 检查支持的特性
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && supportedFeatures.samplerAnisotropy && swapChainAdequate;

        return true;
    }

    VulkanEngine::QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice physicalm_device)
    {
        QueueFamilyIndices indices;
        uint32_t           queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalm_device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalm_device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) // if support graphics command queue
            {
                indices.graphicsFamily = i;
            }

            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) // if support compute command queue
            {
                indices.computeFamily = i;
            }


            VkBool32 isPresentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalm_device,
                i,
                surface,
                &isPresentSupport); // if support surface presentation
            if (isPresentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }
            i++;
        }
        return indices;
    }

    bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
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

    VulkanEngine::SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device)
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

    VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }
        LOG_ERROR("failed to find supported format!");
        return VK_FORMAT_UNDEFINED;
    }

    VkFormat VulkanRenderer::findDepthFormat()
    {
        return findSupportedFormat(
            {
                VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        VkSurfaceFormatKHR format;
        // 每个VkSurfaceFormatKHR结构都包含一个format和一个colorSpace
        // format指定颜色通道和类型

        // 如果只有一种格式，就用RGBA
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            // TODO:如果选择VK_FORMAT_B8G8R8A8_SRGB，就不需要进行gamma矫正了
            format.format = VK_FORMAT_B8G8R8A8_UNORM;
            format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

            return format;
        }

        // 可以选的话
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        // 如果都失效了，就只能选第一个了
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        // 这里不考虑垂直同步，优先选用mailBox
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        // 垂直同步
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR)
            {
                return availablePresentMode;
            }
        }

        // 即时提交，会造成画面撕裂
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        // 让swapChain的图像大小尽量等于窗体大小
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            VkExtent2D actualExtent = { windowWidth, windowHeight };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }
}