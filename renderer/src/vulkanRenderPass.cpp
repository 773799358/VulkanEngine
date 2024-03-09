#include "vulkanRenderPass.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"

namespace VulkanEngine
{
	void VulkanRenderPass::init(VulkanRenderer* vulkanRender, VulkanRenderSceneData* sceneData)
	{
		this->vulkanRender = vulkanRender;
		this->sceneData = sceneData;
	}

}