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

        for (int i = 0; i < 2; i++)
        {
            StaticMesh* cube = sceneData->createCube();
            sceneData->meshes.push_back(cube);
            sceneData->nodes.push_back(cube->node);
        }

        sceneData->setupRenderData();

        mainRenderPass = new MainRenderPass();
        mainRenderPass->init(vulkanRenderer);

        UIRenderPass = new UIPass();
        UIRenderPass->init(vulkanRenderer, mainRenderPass);

        lastFrmeTime = std::chrono::high_resolution_clock::now();
    }

    void Renderer::drawFrame()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration<double, std::milli>(now - lastFrmeTime).count();

        float frameTimer = (float)diff / 1000.0f;

        sceneData->cameraController.processInputEvent(&vulkanRenderer->windowHandler->getEvent(), frameTimer);

        sceneData->updateUniformRenderData();

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
        clearColors[0].color = { {0.2f, 0.2f, 0.2f, 1.0f} };
        clearColors[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = sizeof(clearColors) / sizeof(clearColors[0]);
        renderPassInfo.pClearValues = clearColors;

        VkCommandBuffer currentCommandBuffer = vulkanRenderer->getCurrentCommandBuffer();
        vulkanRenderer->cmdBeginRenderPass(currentCommandBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPass->renderPipelines[0].pipeline);

        for (size_t i = 0; i < sceneData->meshes.size(); i++)
        {
            uint32_t dynamicOffset = i * sizeof(UniformBufferDynamicObject);
            vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPass->renderPipelines[0].layout, 0, 1, &sceneData->descriptor.descriptorSet[vulkanRenderer->currentFrameIndex], 1, &dynamicOffset);

            VkBuffer vertexBuffers[] = { sceneData->meshes[i]->vertexBuffer.buffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(currentCommandBuffer, sceneData->meshes[i]->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            mainRenderPass->drawIndexed(currentCommandBuffer, sceneData->meshes[i]->indices.size());
        }
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
