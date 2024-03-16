#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>
#include <algorithm>
#include <filesystem>

#include "app.hpp"

// 这里注意，要跟SDL库中定义的main函数一致
int main(int argc, char* argv[])
{
    std::string basePath = SDL_GetBasePath();
    basePath = std::filesystem::path(basePath).parent_path().string();

    std::string title = "Vulkan Engine";
    VulkanEngine::App app(title, 2048, 1080);
    app.setBasePath(basePath);
    
    try
    {
        app.init();
        app.loop();
        app.quit();
    } 
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        //system("pause");
        return EXIT_FAILURE;
    }
    
    //system("pause");
    return EXIT_SUCCESS;
}