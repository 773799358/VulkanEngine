#include "macro.hpp"
#include "renderer.hpp"
#include "modelLoader.hpp"

#include <iostream>
#include <map>
#include <set>
#include <functional>

namespace VulkanEngine
{
    bool forward = false;

    Renderer::Renderer()
    {
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::init(Window* window, const std::string& basePath)
    {
        vulkanRenderer = new VulkanRenderer();
        vulkanRenderer->init(window, basePath, forward);
        this->basePath = basePath;

        sceneData = new VulkanRenderSceneData();
        sceneData->init(vulkanRenderer);

        //sceneData->shaderName = "PBR";
        sceneData->shaderName = "DisneyPBR";
        //sceneData->shaderName = "blinn";

        std::string modelPath = basePath + "/resources/models/";

        //modelPath += "post_apocalyptic_telecaster_-_final_gap/scene.gltf";
        //modelPath += "dae_-_bilora_bella_46_camera_-_game_ready_asset.glb"; 
        //modelPath += "reflection_scene.gltf";
        //modelPath += "testModel/scene.gltf";
        //modelPath += "teapot.gltf";
        //modelPath += "10_2k_space_pbr_textures_-_free/scene.gltf";
        //{
        //    modelPath += "ld_textures/scene.gltf";
        //    sceneData->rotate = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        //}
        //{
        //    modelPath += "pack_of_urns/scene.gltf";
        //    sceneData->rotate = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        //}
        {
            modelPath += "awesome_mix_guardians_of_the_galaxy/scene.gltf";
            sceneData->rotate = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        
        Model model(modelPath, sceneData);
         
        //for (int i = 0; i < 2; i++)
        //{
        //    Mesh* cube = sceneData->createCube();
        //    sceneData->meshes.push_back(cube);
        //    sceneData->nodes.push_back(cube->node);
        //}

        sceneData->setupRenderData();

        directionalLightShadowMapPass = new DirectionalLightShadowMapRenderPass();
        directionalLightShadowMapPass->init(vulkanRenderer, sceneData);

        sceneData->createDirectionalLightShadowDescriptorSet(directionalLightShadowMapPass->frameBuffers[0].attachments[0].imageView);
        sceneData->createDeferredUniformDescriptorSet();

        deferredRenderPass = new DeferredRenderPass();
        deferredRenderPass->init(vulkanRenderer, sceneData);

        mainRenderPass = new MainRenderPass();
        mainRenderPass->init(vulkanRenderer, sceneData);

        UIRenderPass = new UIPass();
        if (forward)
        {
            UIRenderPass->init(vulkanRenderer, mainRenderPass, 0, sceneData);
        }
        else
        {
            UIRenderPass->init(vulkanRenderer, deferredRenderPass, deferredRenderPass->renderPipelines.size() - 1, sceneData);
        }

        sceneData->lookAtSceneCenter();

        lastFrmeTime = std::chrono::high_resolution_clock::now();
    }

    void Renderer::drawFrame()
    {
        std::function<void()> passUpdateAfterRecreateSwapchain;
        if (forward)
        {
            passUpdateAfterRecreateSwapchain = std::bind(&MainRenderPass::recreate, mainRenderPass);
        }
        else
        {
            passUpdateAfterRecreateSwapchain = std::bind(&DeferredRenderPass::recreate, deferredRenderPass);
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration<double, std::milli>(now - lastFrmeTime).count();

        float frameTimer = (float)diff / 1000.0f;

        sceneData->cameraController.processInputEvent(&vulkanRenderer->windowHandler->getEvent(), frameTimer);

        sceneData->updateUniformRenderData();

        VkCommandBuffer currentCommandBuffer = vulkanRenderer->getCurrentCommandBuffer();

        if (vulkanRenderer->beginPresent(passUpdateAfterRecreateSwapchain))
        {
            return;
        }

        // shadow
        {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = directionalLightShadowMapPass->renderPass;
            renderPassInfo.framebuffer = directionalLightShadowMapPass->frameBuffers[0].frameBuffer;
            renderPassInfo.renderArea.extent.width = directionalLightShadowMapPass->frameBuffers[0].width;
            renderPassInfo.renderArea.extent.height = directionalLightShadowMapPass->frameBuffers[0].height;
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = { {1.0f} };
            clearValues[1].depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount = clearValues.size();
            renderPassInfo.pClearValues = clearValues.data();

            vulkanRenderer->cmdBeginRenderPass(currentCommandBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, directionalLightShadowMapPass->renderPipelines[0].pipeline);

            VkDeviceSize vertexOffset = 0;
            VkDeviceSize indexOffset = 0;
            for (size_t i = 0; i < sceneData->meshes.size(); i++)
            {
                uint32_t dynamicOffset = i * sizeof(UniformBufferDynamicObject);

                VkDescriptorSet set[1] = { directionalLightShadowMapPass->descriptorInfos[0].descriptorSet };
                vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, directionalLightShadowMapPass->renderPipelines[0].layout, 0, 1, set, 1, &dynamicOffset);

                VkBuffer vertexBuffers[] = { sceneData->vertexResource.buffer };
                VkDeviceSize vertexOffsets[] = { vertexOffset };
                vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, vertexOffsets);

                vkCmdBindIndexBuffer(currentCommandBuffer, sceneData->indexResource.buffer, indexOffset, VK_INDEX_TYPE_UINT32);

                vertexOffset += sizeof(Vertex) * sceneData->meshes[i]->vertices.size();
                indexOffset += sizeof(Uint32) * sceneData->meshes[i]->indices.size();

                directionalLightShadowMapPass->drawIndexed(currentCommandBuffer, sceneData->meshes[i]->indices.size());
            }

            vulkanRenderer->cmdEndRenderPass(currentCommandBuffer);
        }

        // ForwardLighting
        if(forward)
        {
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
        
            vulkanRenderer->cmdBeginRenderPass(currentCommandBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
            vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPass->renderPipelines[0].pipeline);
        
            VkDeviceSize vertexOffset = 0;
            VkDeviceSize indexOffset = 0;
            for (size_t i = 0; i < sceneData->meshes.size(); i++)
            {
                uint32_t dynamicOffset = i * sizeof(UniformBufferDynamicObject);
        
                std::array<VkDescriptorSet, 3> sets = { sceneData->uniformDescriptor.descriptorSet[0], sceneData->meshes[i]->material->descriptorSet, sceneData->directionalLightShadowDescriptor.descriptorSet[0] };
                vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPass->renderPipelines[0].layout, 0, sets.size(), sets.data(), 1, &dynamicOffset);
        
                VkBuffer vertexBuffers[] = { sceneData->vertexResource.buffer };
                VkDeviceSize vertexOffsets[] = { vertexOffset };
                vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, vertexOffsets);
        
                vkCmdBindIndexBuffer(currentCommandBuffer, sceneData->indexResource.buffer, indexOffset, VK_INDEX_TYPE_UINT32);
        
                vertexOffset += sizeof(Vertex) * sceneData->meshes[i]->vertices.size();
                indexOffset += sizeof(Uint32) * sceneData->meshes[i]->indices.size();
        
                mainRenderPass->drawIndexed(currentCommandBuffer, sceneData->meshes[i]->indices.size());
            }
            UIRenderPass->draw(currentCommandBuffer, 0);
        
            vulkanRenderer->cmdEndRenderPass(currentCommandBuffer);
        }
        else
        // DeferredLighting
        {
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = deferredRenderPass->renderPass;
            renderPassInfo.framebuffer = deferredRenderPass->swapChainFrameBuffers[vulkanRenderer->currentFrameIndex];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = vulkanRenderer->swapChainExtent;

            VkClearValue clearColors[7];

            clearColors[0].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
            clearColors[1].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
            float clearColor = 0.0f;
            clearColors[2].color = { { clearColor, clearColor, clearColor, 0.0f } };
            clearColors[3].depthStencil = { 1.0f, 0 };
            clearColors[4].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
            clearColors[5].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
            clearColors[6].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
            renderPassInfo.clearValueCount = sizeof(clearColors) / sizeof(clearColors[0]);
            renderPassInfo.pClearValues = clearColors;

            vulkanRenderer->cmdBeginRenderPass(currentCommandBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredRenderPass->renderPipelines[0].pipeline);

            VkDeviceSize vertexOffset = 0;
            VkDeviceSize indexOffset = 0;
            for (size_t i = 0; i < sceneData->meshes.size(); i++)
            {
                uint32_t dynamicOffset = i * sizeof(UniformBufferDynamicObject);

                std::array<VkDescriptorSet, 2> sets = { sceneData->uniformDescriptor.descriptorSet[0], sceneData->meshes[i]->material->descriptorSet };
                vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredRenderPass->renderPipelines[0].layout, 0, sets.size(), sets.data(), 1, &dynamicOffset);

                VkBuffer vertexBuffers[] = { sceneData->vertexResource.buffer };
                VkDeviceSize vertexOffsets[] = { vertexOffset };
                vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, vertexOffsets);

                vkCmdBindIndexBuffer(currentCommandBuffer, sceneData->indexResource.buffer, indexOffset, VK_INDEX_TYPE_UINT32);

                vertexOffset += sizeof(Vertex) * sceneData->meshes[i]->vertices.size();
                indexOffset += sizeof(Uint32) * sceneData->meshes[i]->indices.size();

                deferredRenderPass->drawIndexed(currentCommandBuffer, sceneData->meshes[i]->indices.size());
            }
            
            {
                vkCmdNextSubpass(currentCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

                vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredRenderPass->renderPipelines[1].pipeline);

                std::array<VkDescriptorSet, 4> sets = { sceneData->directionalLightShadowDescriptor.descriptorSet[0], deferredRenderPass->descriptorInfos[0].descriptorSet, sceneData->deferredUniformDescriptor.descriptorSet[0], sceneData->IBLDescriptor.descriptorSet[0] };
                vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredRenderPass->renderPipelines[1].layout, 0, sets.size(), sets.data(), 0, nullptr);

                deferredRenderPass->draw(currentCommandBuffer, 3);
            }
            // FXAA
            {
                vkCmdNextSubpass(currentCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

                vulkanRenderer->cmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredRenderPass->renderPipelines[2].pipeline);

                std::array<VkDescriptorSet, 1> sets = { deferredRenderPass->descriptorInfos[1].descriptorSet };

                vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredRenderPass->renderPipelines[2].layout, 0, sets.size(), sets.data(), 0, nullptr);

                deferredRenderPass->draw(currentCommandBuffer, 3);
            }
            UIRenderPass->draw(currentCommandBuffer, 0);

            vulkanRenderer->cmdEndRenderPass(currentCommandBuffer);
        }

        vulkanRenderer->endPresent(passUpdateAfterRecreateSwapchain);
    }

    void Renderer::quit()
    {
        mainRenderPass->clear();
        UIRenderPass->clear();
        directionalLightShadowMapPass->clear();
        deferredRenderPass->clear();
        sceneData->clear();
        delete vulkanRenderer;
    }

}
