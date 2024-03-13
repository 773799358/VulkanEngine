cmake_minimum_required(VERSION 3.15)

project(VulkanEngine LANGUAGES CXX DESCRIPTION "VULKAN_ENGINE")

# 编译shader
find_program(GLSLC_PROGRAM glslc REQUIRED)  # 找到可执行文件

message(STATUS "run glslc to compile shaders ...")
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/vs.vert -o ${CMAKE_SOURCE_DIR}/spvs/vs.vert.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/PBR.frag -o ${CMAKE_SOURCE_DIR}/spvs/PBR.frag.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/blinn.frag -o ${CMAKE_SOURCE_DIR}/spvs/blinn.frag.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/DisneyPBR.frag -o ${CMAKE_SOURCE_DIR}/spvs/DisneyPBR.frag.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/directionalLightShadow.vert -o ${CMAKE_SOURCE_DIR}/spvs/directionalLightShadow.vert.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/directionalLightShadow.frag -o ${CMAKE_SOURCE_DIR}/spvs/directionalLightShadow.frag.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/gbuffer.frag -o ${CMAKE_SOURCE_DIR}/spvs/gbuffer.frag.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/deferredLighting.vert -o ${CMAKE_SOURCE_DIR}/spvs/deferredLighting.vert.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_SOURCE_DIR}/shaders/deferredLighting.frag -o ${CMAKE_SOURCE_DIR}/spvs/deferredLighting.frag.spv)
message(STATUS "compile shader OK")

include(cmake/FindVulkan.cmake)
include(cmake/FindSDL2.cmake)
include(cmake/CopyFiles.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(renderer)

add_subdirectory(test)