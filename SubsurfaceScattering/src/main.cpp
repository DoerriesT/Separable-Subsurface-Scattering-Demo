#include <cstdlib>
#include "window/Window.h"
#include "input/UserInput.h"
#include "utility/Timer.h"
#include "vulkan/Renderer.h"

using namespace sss;

int main()
{
	Window window(800, 600, "Subsurface Scattering");
	UserInput userInput;
	vulkan::Renderer renderer(window.getWindowHandle(), 800, 600);

	window.addInputListener(&userInput);

	util::Timer timer;
	uint64_t previousTickCount = timer.getElapsedTicks();
	uint64_t frameCount = 0;

	while (!window.shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		window.pollEvents();
		userInput.input();
		renderer.render();

		double timeDiff = (timer.getElapsedTicks() - previousTickCount) / static_cast<double>(timer.getTickFrequency());
		if (timeDiff > 1.0)
		{
			double fps = frameCount / timeDiff;
			window.setTitle("Subsurface Scattering - " + std::to_string(fps) + " FPS " + std::to_string(1.0 / fps * 1000.0) + " ms");
			previousTickCount = timer.getElapsedTicks();
			frameCount = 0;
		}
		++frameCount;
	}

	return EXIT_SUCCESS;
}