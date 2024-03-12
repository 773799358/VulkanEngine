#pragma once
#include <string>
#include "window.hpp"
#include "vulkanRenderer.hpp"
#include "vulkanRenderPass.hpp"

#include "renderPass_forwardLight.hpp"
#include "renderPass_UI.hpp"
#include "renderPass_directionalLightShadow.hpp"
#include "vulkanScene.hpp"

#include <chrono>

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
        MainRenderPass* mainRenderPass = nullptr;
        DirectionalLightShadowMapRenderPass* directionalLightShadowMapPass = nullptr;

        VulkanRenderSceneData* sceneData = nullptr;

        std::chrono::steady_clock::time_point lastFrmeTime;
    };
}
