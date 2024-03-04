#include "app.hpp"

namespace VulkanEngine
{
	App::App(const std::string& title, int width, int height):title(title), width(width), height(height)
	{
	}

	App::~App()
	{
	}

	void App::setBasePath(const std::string& basePath)
	{
		this->basePath = basePath;
	}

	std::string App::getBasePath()
	{
		return basePath;
	}

	void App::init()
	{
		window.init(title, width, height);
		renderer.init(&window, basePath);
	}

	void App::loop()
	{
		while (!window.shouldClose())
		{
			window.loop();
			renderer.drawFrame();
		}
	}

	void App::quit()
	{
		renderer.quit();
	}

}