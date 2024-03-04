# VulkanEngine

仅用于学习使用，有问题请邮件联系：773799358@qq.com

## 2024.3.3

1. Cmake：版本3.15以上，仅支持windows，后续会对其他平台进行支持
2. 暂不使用 VMA 进行内存管理，后续改进
3. Vulkan initializers 借用官方示例 vks 以减少 VK_STRUCTURE_TYPE_XXX_CREATE_INFO
4. 使用 vulkan.h 后续改为 vulkan.hpp 的可能性不大
5. shader通过cmake execute_process 执行 glslc 进行编译，后续改进
6. 有些三方库跟着Renderer一起编译，后续会拿出去（引擎比较小，编译时间很快，暂时就这样吧，懒得改，优先级比较后面）
7. 暂时只有基础渲染功能，管线支持也比较少，连Renderer都称不上，更不能说是个Engine，希望有持续维护的动力

## 2024.3.4

1. 借用官方示例的部分代码，对device进行封装