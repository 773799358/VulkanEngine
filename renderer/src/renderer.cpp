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

        sceneData = new VulkanRenderSceneData();
        sceneData->init(vulkanRenderer);

        sceneData->meshes.push_back(sceneData->createCube());

        mainRenderPass = new MainRenderPass();
        mainRenderPass->init(vulkanRenderer);

        UIRenderPass = new UIPass();
        UIRenderPass->init(vulkanRenderer, mainRenderPass);
    }

    void Renderer::drawFrame()
    {
        if (vulkanRenderer->beginPresent(std::bind(&MainRenderPass::recreate, mainRenderPass)))
        {
            return;
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mainRenderPass->renderPass;
        renderPassInfo.framebuffer = mainRenderPass->frameBuffers[vulkanRenderer->currentFrameIndex].frameBuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = vulkanRenderer->swapChainExtent;
        VkClearValue clearColors[2];
        clearColors[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearColors[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = sizeof(clearColors) / sizeof(clearColors[0]);
        renderPassInfo.pClearValues = clearColors;

        VkCommandBuffer currentCommandBuffer = vulkanRenderer->getCurrentCommandBuffer();
        vulkanRenderer->cmdBeginRenderPass(currentCommandBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPass->renderPipelines[0].pipeline);

        VkBuffer vertexBuffers[] = { sceneData->meshes[0]->vertexBuffer.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(currentCommandBuffer, sceneData->meshes[0]->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        mainRenderPass->drawIndexed(currentCommandBuffer, sceneData->meshes[0]->indices.size());
        UIRenderPass->draw(currentCommandBuffer, 0);

        vulkanRenderer->cmdEndRenderPass(currentCommandBuffer);

        vulkanRenderer->endPresent(std::bind(&MainRenderPass::recreate, mainRenderPass));
    }

    void Renderer::quit()
    {
        mainRenderPass->clear();
        UIRenderPass->clear();
        sceneData->clear();
        delete vulkanRenderer;
    }

}
