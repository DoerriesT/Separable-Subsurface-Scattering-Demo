#include <cstdlib>
#include "window/Window.h"
#include "input/UserInput.h"
#include "utility/Timer.h"
#include "vulkan/Renderer.h"
#include "input/ArcBallCamera.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace sss;

int main()
{
	Window window(800, 600, "Subsurface Scattering");
	UserInput userInput;
	vulkan::Renderer renderer(window.getWindowHandle(), 800, 600);

	window.addInputListener(&userInput);

	ArcBallCamera camera(glm::vec3(0.0f, 0.2f, 0.0f), 1.0f);

	util::Timer timer;
	uint64_t previousTickCount = timer.getElapsedTicks();
	uint64_t frameCount = 0;

	while (!window.shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		window.pollEvents();
		userInput.input();

		const glm::mat4 viewMatrix = camera.update(userInput.getMousePosDelta() * (userInput.isMouseButtonPressed(InputMouse::BUTTON_RIGHT) ? 1.0f : 0.0f), userInput.getScrollOffset().y);

		glm::mat4 viewProjection;
		{
			glm::mat4 vulkanCorrection =
			{
				{ 1.0f, 0.0f, 0.0f, 0.0f },
				{ 0.0f, -1.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.5f, 0.0f },
				{ 0.0f, 0.0f, 0.5f, 1.0f }
			};
			const glm::mat4 projectionMatrix = glm::perspective(glm::radians(60.0f), 800 / float(600), 0.1f, 50.0f);
			viewProjection = vulkanCorrection * projectionMatrix * viewMatrix;
		}

		renderer.render(viewProjection);

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