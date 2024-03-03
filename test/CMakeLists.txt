add_executable(test)
aux_source_directory(./ test_SRC)
target_sources(test PRIVATE ${test_SRC})
target_link_libraries(test PUBLIC Renderer SDL2 ${Vulkan_LIBRARIES})
target_include_directories(test PUBLIC ${Vulkan_INCLUDE_DIRS}/vulkan)

CopyDLL(test)
CopyShader(test)
CopyResource(test)