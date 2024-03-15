# VulkanEngine

仅用于学习使用，有问题请邮件联系：773799358@qq.com
仅支持windows，因为没有苹果电脑和linux环境，后续会对其他平台进行支持，需要安装vulkan的时候需勾选SDL2（安装包已放入项目，建议勾选全部组件），直接执行build_window.bat即可
然后选择test为启动项，运行，或者进入目录VulkanEngine\build\Release，双击运行test.exe

#### 这个小玩具暂时还处于极度简陋的阶段，无论是封装，内存的管理，还是各种工具的运用（没有使用SPIRV-CROSS）等等，很初级，不过希望能慢慢丰富起来。现阶段主要是锻炼vulkan出错的调试能力（封装越简陋，就越容易出错，刚好锻炼一下调试，把各种常见的验证报错都熟悉一遍，通过报错再去熟悉VulaknAPI）和搭建基本的pass，绘制出来点东西

### 现有功能：
1. 前向管线
2. 非透明物体的直接方向光PBR
3. 前向管线的MSAA
4. 方向光阴影、PCF
5. 延迟管线 + FXAA
6. DisneyPBR + IBL

### 未来可能要实现和优化的部分以及建议笔记：

|  分类    | 描述  | 备注 |
|  ----    | ----  | ---- |
| pipeline种类 |||||
|| 前向 | 点光源、~~方向光~~、~~阴影~~、~~MSAA~~、FORWARD+ |
|| 延迟 | 点光源、~~方向光~~、~~阴影~~、~~FXAA~~ |
|||TAA、延迟后+前向半透明、CSM+VSSM、PBR+IBL、lightmap+probe、SSDO、SSAO、SSR、~~toneMapping~~、grad等等后处理|
| vulkan资源对象管理||||
|| 接入VMA ||
|| 将position属性与其他属性分开储存，加速顶点着色 ||
|| 场景数据内存管理，根据更新频率不同，申请不同的大块内存，一次性提交大量数据，用offset来使用实例数据（mesh数据已经用这个方法实现，因为是暂时是静态场景，一次上传就不更新了），加入ringbuffer | ~~对于uniform缓冲使用VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT标记，避免暂存资源的两次复制~~，使用持久化标记，避免频繁进行map/unmap操作。<br>VkPhysicalDeviceLimits::maxDescriptorSetUniformBuffers：可以绑定到一个描述符集的最大uniform缓冲数量。 <br>对于着色器输入，使用uniform缓冲比storage缓冲更好。| 
|| descriptorSetLayout编码为key，同一pass下按照key和pipeline共同决定draw顺序（材质排序），减少切换|sampler 已经根据mipLevel做key，进行共用|
| pipeline管理 ||||
|| 使用一个pipeline cache创建所有管线对象 | 避免使用衍生管线对象 |
| descriptorSet管理 ||||
|| 对descriptorSetLayout进行编码，构建map，实现复用 | 如果需要动态更新描述符set，需要进行同步，保证数据安全 | 
| 着色器 ||||
|| glsl中使用#define来确定执行路径 ||
|| 管线中使用specialization constants静态确定执行路径 ||
|| 一个vkCmdBindDescriptorSets调用后的多个draw操作需要更新uniform缓冲数据，应该使用动态uniform缓冲offset ||
|| 考虑使用推送常量(push constants)来设置需要每个draw操作更新的数据 ||
|| 只使用推送常量(push constants)更新小于128字节的需要高频更新的数据。如果不能保证数据小于128字节，备用一个动态uniform缓冲 ||
|| 使用SPIRV-Cross完成shader反射，自动创建descriptorSet和layout
| 剔除 ||||
|| 动态角色多，就用八叉树（显然一般引擎都会用八叉树，BVH的重新构建代价有点大），否则用BVH ||
|| 增加遮挡剔除 ||
| command buffer管理 |
|| 尽量不使用secondary command buffers | 对于部分Vulkan实现，需要在一个render pass中的所有指令存储在一个连续的内存块中，这种情况下，驱动可能会在指令执行前将辅助指令缓冲(secondary command buffers)中的数据复制到主指令缓冲(primary command buffers)中。基于这点，建议尽量使用主指令缓冲(primary command buffers)来避免可能的内存复制。如果需要进行多线程渲染，我们推荐优先考虑并行构建主指令缓冲(primary command buffers)。 |
| draw ||||
|| 尽量drawInstance，一次绘制大量对象 ||
| render pass ||||
|| 尽量使用subpass，对移动端有优化 | 需要在render pass开始时清除附着，应该使用VK_ATTACHMENT_LOAD_OP_CLEAR。<br> 在一个sub pass中清除附着，应该调用vkCmdClearAttachments函数。<br> 在render pass外清除附着，应该调用vkCmdClearColorImage和vkCmdClearDepthStencilImage函数。这种方式最不高效。|
    

## 2024.3.3

1. Cmake：版本3.15以上，仅支持windows，后续会对其他平台进行支持，需要安装vulkan的时候勾选SDL2
2. 暂不使用 VMA 进行内存管理，后续改进
3. 使用 vulkan.h 后续改为 vulkan.hpp 的可能性不大
4. shader通过cmake execute_process 执行 glslc 生成 SPV，后续改进使用SPIRV CROSS
5. 有些三方库跟着Renderer一起编译，后续会拿出去（引擎比较小，编译时间很快，暂时就这样吧，懒得改，优先级比较后面）
6. 暂时只有基础渲染功能，管线支持也比较少，连Renderer都称不上，更不能说是个Engine，希望有持续维护的动力

## 2024.3.4

1. 整体结构调整，变得更加合理，框架上更加清晰，更有利于扩展和后续减少代码量
2. 代码基本都堆在了vulkanRenderer里面，后面拆出去，对更多类型进行封装
3. 很多代码都是从tutorial拿过来的，所以不会做队列间共享资源的支持

## 2024.3.5

1. 学习piccolo的vulkan rhi封装
2. 创建了第一个TestRenderPass triangle 还有很多接口没有封装，慢慢来吧

## 2024.3.6

1. 解决了窗口一些事件处理的bug
2. 拆分了两个pass，mainPass和UIPass，后续会将UIPass作为subpass进行绘制
3. 正方体

## 2024.3.7

1. lookat camera controller
2. 用 uniform_buffer_dynamic 更新模型 model 矩阵，uniform_buffer_dynamic存了所有mesh的model矩阵，一次拷贝所有数据
3. 画了两个立方体，锯齿严重，所以加入msaa，后续应该会添加FXAA或者SMAA这种图像空间抗锯齿的选项
4. 把所有第三方库放入项目一起编译太憨了，所以要修改目录结构和cmake，现在在vs中所有项目都是平铺的，丑了点，后面再搞吧
5. 增加了build_window.bat

## 2024.3.8

1. assimp loadModel

## 2024.3.9

1. 平行光Cook-Torrance PBR（没有进行 Kulla-Conty 能量补偿）
2. 对texture包含在文件中，例如：GLB，进行贴图支持，构建image samlper以miplevels为key，进行共用
3. gamma矫正 + toneMapping

## 2024.3.10

1. disney PBR(效果改善很多，但是需要tangent数据)
2. 增加 blinn phone

## 2024.3.11

1. 方向光阴影

## 2024.3.12

1. shadow PCF
2. 延迟渲染（一部分）

## 2024.3.13

1. 延迟渲染（light和后处理都使用subpass）

## 2024.3.14

1. 前向管线使用msaa
2. 延迟管线使用fxaa

## 2024.3.15

1. 天空盒
2. IBL