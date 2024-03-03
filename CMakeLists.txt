cmake_minimum_required(VERSION 3.15)

project(VulkanEngine LANGUAGES CXX DESCRIPTION "VULKAN_ENGINE")

# 编译shader
find_program(GLSLC_PROGRAM glslc REQUIRED)  # 找到可执行文件

message(STATUS "run glslc to compile shaders ...")
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/shader.vert -o ${CMAKE_SOURCE_DIR}/spvs/vert.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/shader.frag -o ${CMAKE_SOURCE_DIR}/spvs/frag.spv)
message(STATUS "compile shader OK")

include(cmake/FindVulkan.cmake)
include(cmake/FindSDL2.cmake)
include(cmake/CopyFiles.cmake)

set(thirdParty_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty)

add_subdirectory(renderer)

add_subdirectory(test)