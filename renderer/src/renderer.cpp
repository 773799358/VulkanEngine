#include "macro.hpp"
#include "renderer.hpp"
#include "vulkanUtil.hpp"

#include <iostream>
#include <map>
#include <set>

namespace VulkanEngine
{
    Renderer::Renderer()
    {
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::init(Window* window, const std::string& basePath)
    {
        vulkanRenderer = new VulkanRenderer();
        vulkanRenderer->init(window);
        this->basePath = basePath;
    }

    void Renderer::drawFrame()
    {
    }

    void Renderer::quit()
    {
        delete vulkanRenderer;
    }

}
