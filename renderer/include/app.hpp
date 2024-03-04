#pragma once
#include <string>
#include "window.hpp"
#include "renderer.hpp"

namespace VulkanEngine
{
	class App
	{
	public:
		App(const std::string& title, int width, int height);
		~App();

		void setBasePath(const std::string& basePath);
		std::string getBasePath();

		void init();
		void loop();
		void quit();

	private:
		std::string basePath;
		int width = 1280;
		int height = 720;
		std::string title = "VulkanEngine";
		Window window;
		Renderer renderer;
	};
}