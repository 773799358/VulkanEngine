#pragma once
#include "SDL2/SDL.h"
#include "vulkanInitializers.hpp"

#include <string>

namespace VulkanEngine
{
    class Renderer
    {
    public:
        Renderer(const std::string& basePath);
        ~Renderer();

        void init();
        void run();

    private:

        void initWindow();
        void onWindowResized(SDL_Window* window, int width, int height);

        void drawFrame();

        void clear();

    private:

        // window
        SDL_Window* window;
        int width = 1024;
        int height = 720;

        // resources
        std::string basePath;
    };
}
