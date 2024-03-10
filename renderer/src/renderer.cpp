#include "macro.hpp"
#include "renderer.hpp"
#include "modelLoader.hpp"

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

        //sceneData->shaderName = "PBR";
        sceneData->shaderName = "DisneyPBR";
        //sceneData->shaderName = "blinn";

        std::string modelPath;

        //modelPath = basePath + "/resources/models/post_apocalyptic_telecaster_-_final_gap/scene.gltf";
        //modelPath = basePath + "/resources/models/dae_-_bilora_bella_46_camera_-_game_ready_asset.glb"; 
        //modelPath = basePath + "/resources/models/reflection_scene.gltf";
        //modelPath = basePath + "/resources/models/the_discobolus_of_myron/scene.gltf";
        //modelPath = basePath + "/resources/models/teapot.gltf";
        //modelPath = basePath + "/resources/models/10_2k_space_pbr_textures_-_free/scene.gltf";
        {
            modelPath = basePath + "/resources/models/awesome_mix_guardians_of_the_galaxy/scene.gltf";
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

        mainRenderPass = new MainRenderPass();
        mainRenderPass->init(vulkanRenderer, sceneData);

        UIRenderPass = new UIPass();
        UIRenderPass->init(vulkanRenderer, mainRenderPass, sceneData);

        sceneData->lookAtSceneCenter();

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

            VkDescriptorSet sets[2] = { sceneData->uniformDescriptor.descriptorSet[0] , sceneData->meshes[i]->material->descriptorSet };
            vulkanRenderer->cmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPass->renderPipelines[0].layout, 0, 2, sets, 1, &dynamicOffset);

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
