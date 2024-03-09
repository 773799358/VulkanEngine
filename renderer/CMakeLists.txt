aux_source_directory(src SRC)

file(GLOB_RECURSE Renderer_header_files ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h*)
source_group("Header Files" FILES ${Renderer_header_files})

file(GLOB_RECURSE Vulkan_header_files ${Vulkan_INCLUDE_DIRS}/vulkan/*.h) # 用c风格代码
source_group("Vulkan" FILES ${Vulkan_header_files})

add_library(Renderer STATIC
    ${SRC} ${Renderer_header_files} 
    ${Vulkan_header_files}
)

target_compile_definitions(Renderer PUBLIC SPDLOG_COMPILED_LIB)
target_compile_definitions(Renderer PUBLIC _GLFW_WIN32)

target_include_directories(Renderer PUBLIC ${CMAKE_CURRENT_LIST_DIR})  # 添加当前文件夹目录
target_include_directories(Renderer PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(Renderer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(Renderer PUBLIC ${Vulkan_INCLUDE_DIRS}/SDL2)

target_link_libraries(Renderer PUBLIC ${Vulkan_LIBRARIES})
target_compile_features(Renderer PUBLIC cxx_std_17)

add_subdirectory(dependencies)