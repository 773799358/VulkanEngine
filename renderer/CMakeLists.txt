aux_source_directory(src SRC)

file(GLOB_RECURSE Renderer_header_files ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h*)
source_group("Header Files" FILES ${Renderer_header_files})

set(glm_DIR ${thirdParty_DIR}/glm)
set(imgui_DIR ${thirdParty_DIR}/imgui)
set(tinygltf_DIR ${thirdParty_DIR}/tinygltf)

file(GLOB_RECURSE glm_files ${glm_DIR}/*.h*)
source_group("glm" FILES ${glm_files})

file(GLOB_RECURSE Vulkan_header_files ${Vulkan_INCLUDE_DIRS}/vulkan/*.h) # 用c风格代码
source_group("Vulkan" FILES ${Vulkan_header_files})

file(GLOB_RECURSE imgui_files ${imgui_DIR}/*.*)
source_group("imgui" FILES ${imgui_files})

file(GLOB_RECURSE tinygltf_files ${tinygltf_DIR}/*.*)
source_group("tinygltf" FILES ${tinygltf_files})

add_library(Renderer STATIC ${SRC} ${Renderer_header_files} ${Vulkan_header_files} ${glm_files} ${imgui_files} ${tinygltf_files})

target_include_directories(Renderer PUBLIC ${CMAKE_CURRENT_LIST_DIR})  # 添加当前文件夹目录
target_include_directories(Renderer PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(Renderer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(Renderer PUBLIC ${thirdParty_DIR})

target_link_libraries(Renderer PUBLIC ${Vulkan_LIBRARIES})
target_compile_features(Renderer PUBLIC cxx_std_17)