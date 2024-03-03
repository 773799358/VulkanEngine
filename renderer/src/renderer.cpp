#include "SDL2/SDL_vulkan.h"
#include "renderer.hpp"

namespace VulkanEngine
{
	Renderer::Renderer(const std::string& basePash)
	{
		this->basePath = basePash + "/";
	}

	Renderer::~Renderer()
	{

	}

	void Renderer::init()
	{
		initWindow();
	}

	void Renderer::run()
	{
		bool shouldClose = false;
		SDL_Event event;
		bool pauseRender = false;
		while (!shouldClose)
		{
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_QUIT)
				{
					shouldClose = true;
				}

				if (event.type == SDL_WINDOWEVENT)
				{
					if (event.window.event == SDL_WINDOWEVENT_RESIZED)
					{
						int newW = event.window.data1;
						int newH = event.window.data2;
						onWindowResized(window, newW, newH);
					}
					if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
					{
						pauseRender = true;
					}
					if (event.window.event == SDL_WINDOWEVENT_RESTORED)
					{
						int newW = event.window.data1;
						int newH = event.window.data2;
						pauseRender = false;
						onWindowResized(window, newW, newH);
					}
				}
			}

			if (!pauseRender)
			{
				drawFrame();
			}
		}
	}

	void Renderer::initWindow()
	{
		SDL_Init(SDL_INIT_EVERYTHING);

		window = SDL_CreateWindow("Vulkan Engine",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height,
			SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	}

	void Renderer::onWindowResized(SDL_Window* window, int width, int height)
	{
	}

	void Renderer::drawFrame()
	{
	}

	void Renderer::clear()
	{
	}

}