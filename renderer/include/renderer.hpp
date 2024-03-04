#pragma once
#include "SDL2/SDL.h"
#include "vulkanInitializers.hpp"

#include <string>

namespace VulkanEngine
{
    struct VulkanDevice;
    class Renderer
    {
    public:
        Renderer(const std::string& basePath);
        ~Renderer();

        void init();
        void run();

        static uint8_t const maxFramesInFlight{ 3 };

    private:

        struct QueueFamilyIndices
        {
            int graphicsFamily = -1;
            int presentFamily = -1;
            bool isComplete()
            {
                return graphicsFamily >= 0 && presentFamily >= 0;
            }
        };

        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        void initWindow();
        void onWindowResized(SDL_Window* window, int width, int height);

        void initVulkan();

        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();
        void createInstance();

        void createSurface();

        void pickPhysicalDevice();
        Renderer::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        Renderer::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        bool isDeviceSuitable(VkPhysicalDevice device);

        void createLogicDevice();

        void drawFrame();

        void clear();

    private:

        // instance
        VkInstance instance;
        std::vector<std::string> supportedInstanceExtensions;

        // window
        SDL_Window* window;
        int width = 1024;
        int height = 720;

        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice;
        
        VulkanDevice* vulkanDevice = nullptr;

        // resources
        std::string basePath;
    };
}
