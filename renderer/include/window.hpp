#pragma once

#include "SDL2/SDL.h"
#include <string>
#include <vector>

namespace VulkanEngine
{
	class Window
	{
	public:
		Window();
		~Window();

		void init(const std::string& title, int width, int height);
		SDL_Window* getWindow();
		std::vector<int> getWindowSize();
		void loop();
		bool shouldClose();

	private:
		SDL_Window* window = nullptr;
		SDL_Event event;
		int width = 0;
		int height = 0;
		
		bool isShouldClose = false;
	};
}