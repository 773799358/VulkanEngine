#pragma once
#include "SDL2/SDL.h"
#include "vulkanInitializers.hpp"
#include "vulkanDevice.hpp"
#include "vulkanSwapChain.hpp"

#include <string>

namespace VulkanEngine
{
    struct SwapChainSupportDetails;
    class Renderer
    {
    public:
        Renderer(const std::string& basePath);
        ~Renderer();

        void init();
        void run();

        static uint8_t const maxFramesInFlight{ 3 };

    private:

        void initWindow();
        void onWindowResized(SDL_Window* window, int width, int height);

        void initVulkan();

        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();
        void createInstance();

        void createSurface();

        void pickPhysicalDevice();
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        bool isDeviceSuitable(VkPhysicalDevice device);

        void createLogicDevice();

        void createSwapChain();

        void drawFrame();

        void clear();

    private:

        // instance
        VkInstance instance;
        std::vector<std::string> supportedInstanceExtensions;

        // window
        SDL_Window* window;
        uint32_t width = 1024;
        uint32_t height = 720;

        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice;
        
        std::unique_ptr<VulkanDevice> vulkanDevice;
        std::unique_ptr<VulkanSwapChain> vulkanSwapChain;

        // resources
        std::string basePath;
    };
}
