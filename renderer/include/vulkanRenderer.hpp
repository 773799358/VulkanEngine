﻿#pragma once
#include "window.hpp"
#include "vulkan/vulkan.h"
#include "vulkanStruct.hpp"
#include <array>
#include <functional>

namespace VulkanEngine
{
    struct viewport
    {
        float x;
        float y;
        float width;
        float height;
        float minDepth;
        float maxDepth;
    };

    // API
    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer();

        void init(Window* window, const std::string& basePath);

        // 如果recreateSwapChain，需要对与此大小相关的资源进行重新创建
        // 返回值代表是否重建了交换链
        bool beginPresent(std::function<void()> passUpdateAfterRecreateSwapchain);
        void endPresent(std::function<void()> passUpdateAfterRecreateSwapchain);

        // command
        void cmdBeginRenderPass(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo randerPassBegin, VkSubpassContents contents);
        void cmdEndRenderPass(VkCommandBuffer commandBuffer);
        void cmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);

        // resource
        VkShaderModule createShaderModule(const std::vector<char>& code);
        uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

        void createImage(
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

        VkImageView createImageView(
            VkImage& image,
            VkFormat format,
            VkImageAspectFlags imageAspectFlags,
            VkImageViewType viewType,
            uint32_t layoutCount,
            uint32_t miplevels);

    public:
        std::string basePath = "";
        Window* windowHandler = nullptr;
        SDL_Window* window = nullptr;
        uint32_t windowWidth = 0;
        uint32_t windowHeight = 0;
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkSurfaceKHR surface;
        VkDevice device;
        VkQueue presentQueue;
        QueueFamilyIndices queueIndices;

        viewport viewport;
        VkRect2D scissor;

        // 一些初始化的验证和配置
        bool enableValidationLayers = true;
        bool enableDebugUtilsLabel = true;
        VkDebugUtilsMessengerEXT debugMessenger;

        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        bool isDeviceSuitable(VkPhysicalDevice physicalDevice);
        VulkanEngine::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalm_device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        VulkanEngine::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkFormat findDepthFormat();
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    private:

        // init
        void createInstance();
        void setupDebugMessenger();
        void createWindowSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
        void createCommandBuffers();
        void createDescriptorPool();
        void createSyncPrimitives();

        // swapChain
        void clearSwapChain();
        void recreateSwapchain();
        void createSwapchain();
        void createSwapchainImageViews();
        void createFramebufferImageAndView();

    public:
        uint32_t vulkanAPIVersion = VK_API_VERSION_1_0;

        // queue
        VkQueue graphicsQueue;
        VkQueue computeQueue;

        // swapChain
        static const int MAX_FRAMES_IN_FLIGHT = 3;
        uint32_t currentSwapChainImageIndex = 0;
        uint32_t currentFrameIndex = 0;

        VkSwapchainKHR swapChain;
        VkFormat swapChainImageFormat;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
        VkExtent2D swapChainExtent;

        // depth
        VkFormat depthImageFormat;
        VkImage depthImage;
        VkImageView depthImageView;
        VkDeviceMemory depthImageMemory;

        // command
        // TODO:不知道多个pool会对性能产生多少影响，官方推荐pool进行reset更好（而不是单独reset某个commandBuffer）
        VkCommandPool mainCommandPool;          // 默认图形command pool
        std::array<VkCommandPool, MAX_FRAMES_IN_FLIGHT> commandPools; // inflight command pool
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers;

        // descriptor
        uint32_t maxVertexBlendingMeshCount = 256;
        uint32_t maxMaterialCount = 256;
        VkDescriptorPool descriptorPool;

        // sync
        VkSemaphore imageAvailableForRenderSemaphores[MAX_FRAMES_IN_FLIGHT];
        VkSemaphore imageFinishedForPresentSemaphores[MAX_FRAMES_IN_FLIGHT];
        VkSemaphore imageAvailableForTextureCopySemaphores[MAX_FRAMES_IN_FLIGHT];
        VkFence isFrameInFlightFences[MAX_FRAMES_IN_FLIGHT];
    };
}