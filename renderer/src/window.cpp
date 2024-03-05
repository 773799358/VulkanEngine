#include "window.hpp"
#include "macro.hpp"
#include "imgui/imgui_impl_sdl2.h"

namespace VulkanEngine
{
	Window::Window()
	{

	};

	Window::~Window()
	{
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	void Window::init(const std::string& title, int width, int height)
	{
		SDL_Init(SDL_INIT_EVERYTHING);

		window = SDL_CreateWindow(title.c_str(),
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height,
			SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

		if (!window)
		{
			LOG_FATAL("failed to create window!");
			SDL_DestroyWindow(window);
			SDL_Quit();
			return;
		}

		this->width = width;
		this->height = height;
	}

	SDL_Window* Window::getWindow()
	{
		return window;
	}

	std::vector<int> Window::getWindowSize()
	{
		return std::vector<int> { width, height };
	}

	void Window::loop()
	{
		SDL_PollEvent(&event);
		if (event.type == SDL_QUIT)
		{
			isShouldClose = true;
		}

		if (event.type == SDL_WINDOWEVENT)
		{
			if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_RESTORED)
			{
				width = event.window.data1;
				height = event.window.data2;
			}
			if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
			{
				width = 0;
				height = 0;
			}
		}

		ImGui_ImplSDL2_ProcessEvent(&event);
	}

	bool Window::shouldClose()
	{
		return isShouldClose;
	}

}