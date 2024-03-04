#pragma once
#include <string>
#include "window.hpp"
#include "vulkanRenderer.hpp"

namespace VulkanEngine
{
    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        void init(Window* window, const std::string& basePath);
        void drawFrame();
        void quit();
    private:
        VulkanRenderer* vulkanRenderer = nullptr;
        std::string basePath;
    };
}
