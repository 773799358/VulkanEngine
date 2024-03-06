#include "vulkanRenderPass.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"

namespace VulkanEngine
{
	void VulkanRenderPass::init(VulkanRenderer* vulkanRender)
	{
		this->vulkanRender = vulkanRender;
	}

}