#include "macro.hpp"
#include "renderer.hpp"

#include <iostream>
#include <map>
#include <set>
#include <functional>

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
        vulkanRenderer->init(window, basePath);
        this->basePath = basePath;

        testRenderPass = new TestRenderPass();
        testRenderPass->init(vulkanRenderer);

        UIRenderPass = new UIPass();
        UIRenderPass->init(vulkanRenderer, testRenderPass);
    }

    void Renderer::drawFrame()
    {
        if (vulkanRenderer->beginPresent(std::bind(&TestRenderPass::recreate, testRenderPass)))
        {
            return;
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = testRenderPass->renderPass;
        renderPassInfo.framebuffer = testRenderPass->swapChainFrameBuffers[vulkanRenderer->currentFrameIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = vulkanRenderer->swapChainExtent;
        VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        VkCommandBuffer currentCommandBuffer = vulkanRenderer->getCurrentCommandBuffer();
        vulkanRenderer->cmdBeginRenderPass(currentCommandBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, testRenderPass->renderPipelines[0].pipeline);

        testRenderPass->draw(currentCommandBuffer);
        UIRenderPass->draw(currentCommandBuffer);

        vulkanRenderer->cmdEndRenderPass(currentCommandBuffer);

        vulkanRenderer->endPresent(std::bind(&TestRenderPass::recreate, testRenderPass));
    }

    void Renderer::quit()
    {
        testRenderPass->clear();
        UIRenderPass->clear();
        delete vulkanRenderer;
    }

}
