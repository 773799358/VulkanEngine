#include "vulkanRenderer.hpp"
#include "macro.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#include "vulkanUtil.hpp"

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
        vkDeviceWaitIdle(device);

        clearSwapChain();

        for (auto& iter = mipmapSamplerMap.begin(); iter != mipmapSamplerMap.end(); iter++)
        {
            vkDestroySampler(device, iter->second, nullptr);
        }
        mipmapSamplerMap.clear();

        vkDestroySampler(device, nearestSampler, nullptr);
        vkDestroySampler(device, linearSampler, nullptr);

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

    void VulkanRenderer::init(Window* window, const std::string& basePath, bool msaa)
    {
        this->basePath = basePath + "/";

        this->msaa = msaa;

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

    bool VulkanRenderer::beginPresent(std::function<void()> passUpdateAfterRecreateSwapchain)
    {
        // 等待上次commandBuffer执行完毕，否则会出现命令堆积
        vkWaitForFences(device, 1, &isFrameInFlightFences[currentFrameIndex], VK_TRUE, UINT64_MAX);
        
        // 重置commandPool，进行重新录制
        VK_CHECK_RESULT(vkResetCommandPool(device, commandPools[currentFrameIndex], 0));

        // 第三个参数指定获取有效图像的操作timeout，单位纳秒。我们使用64位无符号最大值禁止timeout。
        VkResult acquireNextImageResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableForRenderSemaphores[currentFrameIndex], VK_NULL_HANDLE, &currentSwapChainImageIndex);

        if (VK_ERROR_OUT_OF_DATE_KHR == acquireNextImageResult)
        {
            // 交换链与surface不一致
            recreateSwapchain();
            passUpdateAfterRecreateSwapchain();
            return true;
        }
        else if (VK_SUBOPTIMAL_KHR == acquireNextImageResult)
        {
            // VK_SUBOPTIMAL_KHR 交换链可以使用，但是surface属性不匹配
            recreateSwapchain();
            passUpdateAfterRecreateSwapchain();

            // 执行一个空提交，来重置semaphore的状态
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };   // 等待上个pass执行结束
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableForRenderSemaphores[currentFrameIndex];
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 0;
            submitInfo.pCommandBuffers = nullptr;
            submitInfo.signalSemaphoreCount = 0;
            submitInfo.pSignalSemaphores = nullptr;

            VkResult resetFences = vkResetFences(device, 1, &isFrameInFlightFences[currentFrameIndex]);
            if (VK_SUCCESS != resetFences)
            {
                LOG_ERROR("failed to reset fence!");
                return false;
            }

            VkResult queueSubmit = vkQueueSubmit(graphicsQueue, 1, &submitInfo, isFrameInFlightFences[currentFrameIndex]);
            if (VK_SUCCESS != queueSubmit)
            {
                LOG_ERROR("failed to queue submit!");
                return false;
            }
            currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
            return true;
        }
        else
        {
            if (VK_SUCCESS != acquireNextImageResult)
            {
                LOG_ERROR("failed to acquire next image!");
                return false;
            }
        }

        // TODO:这里每次都重新录制，对于未修改的情况是否可以优化？
        // 开始录制
        VkCommandBufferBeginInfo commandBufferBeginInfo = {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 命令缓冲将在执行一次后立即重新记录。
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: 这是一个辅助缓冲，它限制在在一个渲染通道中。
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : 命令缓冲也可以重新提交，同时它也在等待执行。
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;

        VkResult beginCommandBuffer = vkBeginCommandBuffer(commandBuffers[currentFrameIndex], &commandBufferBeginInfo);

        if (VK_SUCCESS != beginCommandBuffer)
        {
            LOG_ERROR("failed to begin command buffer!");
            return false;
        }
        return false;
    }

    void VulkanRenderer::endPresent(std::function<void()> passUpdateAfterRecreateSwapchain)
    {
        // 结束录制
        VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffers[currentFrameIndex]));
        
        std::array<VkSemaphore, 1> signalSemaphores = {
            //imageAvailableForTextureCopySemaphores[currentFrameIndex],
            imageFinishedForPresentSemaphores[currentFrameIndex]
        };

        // 表示执行到 WaitDstStageMask 要停下，等待 WaitSemaphores 状态为 signaled，执行完毕后 SignalSemaphores 的状态会变为 signaled
        VkSemaphore waitSemaphores[] = { imageAvailableForRenderSemaphores[currentFrameIndex] };

        // 之前的pass到了输出图像到附件的阶段进行等待swapChain准备好image
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrameIndex];
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        VK_CHECK_RESULT(vkResetFences(device, 1, &isFrameInFlightFences[currentFrameIndex]));	// 将fence重置为 unsignaled

        // 可以一次性做大量提交
        VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, isFrameInFlightFences[currentFrameIndex]));

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &imageFinishedForPresentSemaphores[currentFrameIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &currentSwapChainImageIndex;
        presentInfo.pResults = nullptr;

        VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

        // 有可能是因为swapChain和surface window不匹配了
        if (VK_ERROR_OUT_OF_DATE_KHR == presentResult || VK_SUBOPTIMAL_KHR == presentResult)
        {
            recreateSwapchain();
            passUpdateAfterRecreateSwapchain();
            return;
        }
        else
        {
            if (VK_SUCCESS != presentResult)
            {
                LOG_ERROR("failed to present!");
                return;
            }
        }

        currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
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

        _vkCmdBeginDebugUtilsLabelEXT =
            (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
        _vkCmdEndDebugUtilsLabelEXT =
            (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
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
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

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

        windowWidth = swapChainExtent.width;
        windowHeight = swapChainExtent.height;

        scissor = { {0, 0}, {swapChainExtent.width, swapChainExtent.height} };
        viewport = { 0.0f, 0.0f, (float)windowWidth, (float)windowHeight, 0.0f, 1.0f };
    }

    void VulkanRenderer::clearSwapChain()
    {
        vkDestroyImage(device, depthImage, nullptr);
        vkDestroyImageView(device, depthImageView, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

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

        vkDeviceWaitIdle(device);

        VkResult resWaitForFences = vkWaitForFences(device, MAX_FRAMES_IN_FLIGHT, isFrameInFlightFences, VK_TRUE, UINT64_MAX);
        if (VK_SUCCESS != resWaitForFences)
        {
            LOG_ERROR("WaitForFences failed");
            return;
        }

        currentFrameIndex = 0;

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
            swapChainImageViews[i] = createImageView(
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
        // TODO:
        msaaSamples = VulkanUtil::getMaxUsableSampleCount(physicalDevice);
        if (!msaa)
        {
            msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        }
        //if (msaaSamples > VK_SAMPLE_COUNT_4_BIT)
        //{
        //    msaaSamples = VK_SAMPLE_COUNT_4_BIT;
        //}

        createImage(
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
            1,
            msaaSamples);

        depthImageView = createImageView(
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
        LOG_DEBUG("validation layer: \n \t {}", pCallbackData->pMessage);

        return VK_FALSE;
    }

    void VulkanRenderer::pushEvnet(VkCommandBuffer& commandBuffer, const char* name, const float* color)
    {
        if (enableValidationLayers)
        {
            VkDebugUtilsLabelEXT label_info;
            label_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label_info.pNext = nullptr;
            label_info.pLabelName = name;
            for (int i = 0; i < 4; ++i)
                label_info.color[i] = color[i];
            
            _vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label_info);
        }
    }

    void VulkanRenderer::popEvent(VkCommandBuffer& commandBuffer)
    {
        if (enableValidationLayers)
        {
            _vkCmdEndDebugUtilsLabelEXT(commandBuffer);
        }
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

    VkCommandBuffer VulkanRenderer::getCurrentCommandBuffer()
    {
        return commandBuffers[currentFrameIndex];
    }

    VkCommandBuffer VulkanRenderer::beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mainCommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// 提交一次就不用了

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);				// 阻塞，直到命令完毕

        vkFreeCommandBuffers(device, mainCommandPool, 1, &commandBuffer);
    }

    void VulkanRenderer::cmdBeginRenderPass(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo randerPassBegin, VkSubpassContents contents)
    {
        vkCmdBeginRenderPass(commandBuffer, &randerPassBegin, contents);
    }

    void VulkanRenderer::cmdEndRenderPass(VkCommandBuffer commandBuffer)
    {
        vkCmdEndRenderPass(commandBuffer);
    }

    void VulkanRenderer::cmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
    {
        vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }

    void VulkanRenderer::cmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout pipelineLayout, uint32_t firstSet, uint32_t descriptorSetCount, VkDescriptorSet* descriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
    {
        vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, pipelineLayout, firstSet, descriptorSetCount, descriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }

    VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();

        // 需要满足uint32_t的对齐要求
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

        return shaderModule;
    }

    uint32_t VulkanRenderer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);		// 查询可用的内存类型
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if (typeFilter & (1 << i))	// 把1左移i位就是类型
            {
                if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    return i;
                }
            }
        }

        LOG_ERROR("failed to find suitable memory type!");
        return 0;
    }

    void VulkanRenderer::createImage(uint32_t imageWidth, uint32_t imageHeight, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& memory, VkImageCreateFlags imageCreateFlags, uint32_t arrayLayers, uint32_t miplevels, VkSampleCountFlagBits numSamples)
    {
        VkImageCreateInfo imageCI{};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.flags = imageCreateFlags;
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.extent.width = imageWidth;
        imageCI.extent.height = imageHeight;
        imageCI.extent.depth = 1;
        imageCI.mipLevels = miplevels;
        imageCI.arrayLayers = arrayLayers;
        imageCI.format = format;								// UNORM 格式表示无符号的归一化格式，其中值从0到255（对于8位）被解释为从0.0到1.0的浮点数
        imageCI.tiling = imageTiling;							// VK_IMAGE_TILING_LINEAR: 纹素基于行主序的布局，如pixels数组
                                                                // VK_IMAGE_TILING_OPTIMAL: 纹素基于具体的实现来定义布局，以实现最佳访问
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// VK_IMAGE_LAYOUT_UNDEFINED: GPU不能使用，第一个变换将丢弃纹素。
        imageCI.usage = imageUsageFlags;
        imageCI.samples = numSamples;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &image));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, memoryPropertyFlags);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        {
            LOG_ERROR("failed to allocate image memory!");
            return;
        }

        vkBindImageMemory(device, image, memory, 0);
    }

    void VulkanRenderer::createTextureImage(VkImage& image, VkImageView& imageView, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, void* pixels, uint32_t miplevels)
    {
        if (!pixels)
        {
            return;
        }

        VkDeviceSize imageSize = width * height * 4;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

        // 要生成mipmap，image既是目标又是源
        createImage(width, height, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory, 0, 1, miplevels);

        transitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        copyBufferToImage(stagingBuffer, image, width, height, 1);
        transitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        
        generateMipmaps(image, format, width, height, miplevels);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        imageView = createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, miplevels);
    }

    void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        // 像素紧密排列
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = layerCount;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent =
        {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage
        (
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanRenderer::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
    {
        // 检查是否支持线性插值
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            LOG_ERROR("texture image format does not support linear blitting!");
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        for (uint32_t i = 1; i < mipLevels; i++)
        {
            VkImageBlit imageBlit{};
            imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.srcSubresource.layerCount = 1;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = std::max((int32_t)(texWidth >> (i - 1)), 1);
            imageBlit.srcOffsets[1].y = std::max((int32_t)(texHeight >> (i - 1)), 1);
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.dstSubresource.layerCount = 1;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = std::max((int32_t)(texWidth >> i), 1);
            imageBlit.dstOffsets[1].y = std::max((int32_t)(texHeight >> i), 1);
            imageBlit.dstOffsets[1].z = 1;

            VkImageSubresourceRange mipSubRange{};
            mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubRange.baseMipLevel = i;
            mipSubRange.levelCount = 1;
            mipSubRange.layerCount = 1;

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = mipSubRange;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);

            vkCmdBlitImage(commandBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &imageBlit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

        VkImageSubresourceRange mipSubRange{};
        mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipSubRange.baseMipLevel = 0;
        mipSubRange.levelCount = mipLevels;
        mipSubRange.layerCount = 1;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = mipSubRange;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    VkImageView VulkanRenderer::createImageView(VkImage& image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageViewType viewType, uint32_t layoutCount, uint32_t miplevels)
    {
        VkImageViewCreateInfo imageViewCI = {};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCI.image = image;
        imageViewCI.viewType = viewType;
        imageViewCI.format = format;
        imageViewCI.subresourceRange.aspectMask = imageAspectFlags;
        imageViewCI.subresourceRange.baseMipLevel = 0;
        imageViewCI.subresourceRange.levelCount = miplevels;
        imageViewCI.subresourceRange.baseArrayLayer = 0;
        imageViewCI.subresourceRange.layerCount = layoutCount;

        VkImageView imageView;
        VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &imageView));

        return imageView;
    }

    VkSampler VulkanRenderer::getOrCreateMipmapSampler(uint32_t miplevles)
    {
        VkSampler sampler;
        auto& findSmapler = mipmapSamplerMap.find(miplevles);
        if (findSmapler != mipmapSamplerMap.end())
        {
            return findSmapler->second;
        }

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;			// 使用归一化坐标

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = static_cast<float>(0);
        samplerInfo.maxLod = static_cast<float>(miplevles);

        VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));

        mipmapSamplerMap.insert(std::make_pair(miplevles, sampler));
        return sampler;
    }

    VkSampler VulkanRenderer::getOrCreateNearestSampler()
    {
        if (linearSampler == VK_NULL_HANDLE)
        {
            VkSamplerCreateInfo samplerInfo{};

            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 16; // close :1.0f
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 8.0f; // TODO: irradianceTextureMiplevels
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &linearSampler));
        }
        return linearSampler;
    }

    VkSampler VulkanRenderer::getOrCreateLinearSampler()
    {
        if (linearSampler == VK_NULL_HANDLE)
        {
            VkSamplerCreateInfo samplerInfo{};

            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 16; // close :1.0f
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 8.0f; // TODO: irradianceTextureMiplevels
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &linearSampler));
        }
        return linearSampler;
    }

    void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// 不被各种队列共享

        VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);			// 查询内存需求

        // 真正分配内存
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;		// 查询真正大小，具体看对齐情况
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory));

        vkBindBufferMemory(device, buffer, bufferMemory, 0);		// 如果偏移量non-zero，需要能够被memRequirements.alignment整除
    }

    void VulkanRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanRenderer::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layoutCount, uint32_t miplevels, VkImageAspectFlags aspectMask)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = miplevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        // for getGuidAndDepthOfMouseClickOnRenderSceneForUI() get depthimage
        else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        // for generating mipmapped image
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else
        {
            LOG_ERROR("unsupported layout transition!");
            return;
        }

        vkCmdPipelineBarrier
        (
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

}