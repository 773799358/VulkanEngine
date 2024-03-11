# 第三方库
set(third_party_folder "ThirdParty")
set(Dependencies ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(Renderer PUBLIC ${Dependencies})

# spdlog
if(NOT TARGET spdlog)
    add_subdirectory(spdlog)
    set_target_properties(spdlog PROPERTIES FOLDER ${third_party_folder}/spdlog)

    target_include_directories(Renderer PRIVATE ${Dependencies}/spdlog/include)
    target_link_libraries(Renderer PRIVATE spdlog)
endif()

# imgui
if(NOT TARGET imgui)
    set(imgui_SOURCE_DIR_ ${Dependencies}/imgui)

    file(GLOB imgui_sources CONFIGURE_DEPENDS  "${imgui_SOURCE_DIR_}/*.cpp")
    file(GLOB imgui_impl CONFIGURE_DEPENDS  
    "${imgui_SOURCE_DIR_}/backends/imgui_impl_sdl2.cpp" 
    "${imgui_SOURCE_DIR_}/backends/imgui_impl_sdl2.h"
    "${imgui_SOURCE_DIR_}/backends/imgui_impl_vulkan.cpp" 
    "${imgui_SOURCE_DIR_}/backends/imgui_impl_vulkan.h")
    add_library(imgui STATIC ${imgui_sources} ${imgui_impl})
    target_include_directories(imgui PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR_}>)
    target_include_directories(imgui PUBLIC $<BUILD_INTERFACE:${vulkan_include}>)
    target_include_directories(imgui PUBLIC $<BUILD_INTERFACE:${Vulkan_INCLUDE_DIRS}/SDL2>)
    target_link_libraries(imgui PUBLIC SDL2 ${vulkan_lib})
    set_target_properties(imgui PROPERTIES FOLDER ${third_party_folder}/imgui)

    target_include_directories(Renderer PRIVATE ${Dependencies}/imgui/include)
    target_link_libraries(Renderer PRIVATE imgui)
endif()

#glm
if(NOT TARGET glm)
    add_library(glm STATIC ${Dependencies}/glm/glm.cpp)
    set_target_properties(glm PROPERTIES FOLDER ${third_party_folder}/glm)

    target_include_directories(Renderer PRIVATE ${Dependencies}/glm)
    target_link_libraries(Renderer PRIVATE glm)
endif()

#assimp
if(NOT TARGET assimp)
    add_subdirectory(assimp)
    set_target_properties(assimp PROPERTIES FOLDER ${third_party_folder}/assimp)

    target_include_directories(Renderer PRIVATE ${Dependencies}/assimp/include)
    target_link_libraries(Renderer PRIVATE assimp)
endif()

#stb
if(NOT TARGET stb)
    file(GLOB stb_sources CONFIGURE_DEPENDS  "${Dependencies}/stb/*.h")
    add_library(stb INTERFACE ${stb_sources})
    set_target_properties(stb PROPERTIES FOLDER ${third_party_folder}/stb)

    target_include_directories(stb INTERFACE ${Dependencies}/stb)
    target_link_libraries(Renderer PRIVATE stb)
endif()