#pragma once
#include <string>
#include "window.hpp"
#include "vulkanRenderer.hpp"
#include "vulkanRenderPass.hpp"

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

        std::string basePath;
        VulkanRenderer* vulkanRenderer = nullptr;

        UIPass* UIRenderPass = nullptr;
        TestRenderPass* testRenderPass = nullptr;
    };
}
