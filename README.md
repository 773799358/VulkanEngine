# VulkanEngine

仅用于学习使用，有问题请邮件联系：773799358@qq.com
仅支持windows，因为没有苹果电脑和linux环境，后续会对其他平台进行支持，需要安装vulkan的时候需勾选SDL2

## 2024.3.3

1. Cmake：版本3.15以上，仅支持windows，后续会对其他平台进行支持，需要安装vulkan的时候勾选SDL2
2. 暂不使用 VMA 进行内存管理，后续改进
3. 使用 vulkan.h 后续改为 vulkan.hpp 的可能性不大
4. shader通过cmake execute_process 执行 glslc 进行编译，后续改进
5. 有些三方库跟着Renderer一起编译，后续会拿出去（引擎比较小，编译时间很快，暂时就这样吧，懒得改，优先级比较后面）
6. 暂时只有基础渲染功能，管线支持也比较少，连Renderer都称不上，更不能说是个Engine，希望有持续维护的动力

## 2024.3.4

1. 不借用了，自己全都写一遍，比较好，借用的接口也不太熟悉，会忘记调用
2. 想用VMA，看了一下Piccolo的用法
3. 整体结构调整，变得更加合理，框架上更加清晰，更有利于扩展和后续减少代码量
4. 代码基本都堆在了vulkanRenderer里面，后面拆出去，对更多类型进行封装
5. 都多代码都是从tutorial拿过来的，所以不会做队列间共享资源的支持

## 2024.3.5

1. 学习piccolo的vulkan rhi封装
2. 创建了第一个TestRenderPass triangle 还有很多接口没有封装，慢慢来吧

## 2024.3.6

1. 解决了窗口一些事件处理的bug
2. 拆分了两个pass，mainPass和UIPass，后续会将UIPass作为subpass进行绘制
3. 正方体

## 2024
1. lookat camera controller