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
	uint32_t width = 1600;
	uint32_t height = 900;
	Window window(width, height, "Subsurface Scattering");
	UserInput userInput;
	vulkan::Renderer renderer(window.getWindowHandle(), width, height);

	window.addInputListener(&userInput);

	ArcBallCamera camera(glm::vec3(0.0f, 0.25f, 0.0f), 1.0f);

	util::Timer timer;
	uint64_t previousTickCount = timer.getElapsedTicks();
	uint64_t frameCount = 0;

	float lightTheta = glm::radians(60.0f);

	while (!window.shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		window.pollEvents();
		userInput.input();
		camera.update(userInput.getMousePosDelta() * (userInput.isMouseButtonPressed(InputMouse::BUTTON_RIGHT) ? 1.0f : 0.0f), userInput.getScrollOffset().y);

		lightTheta += static_cast<float>(userInput.isKeyPressed(InputKey::LEFT)) * static_cast<float>(timer.getTimeDelta());
		lightTheta -= static_cast<float>(userInput.isKeyPressed(InputKey::RIGHT)) * static_cast<float>(timer.getTimeDelta());

		glm::vec3 lightPos(glm::cos(lightTheta), 0.2f, glm::sin(lightTheta));

		glm::mat4 vulkanCorrection =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 1.0f }
		};

		const glm::mat4 viewMatrix = camera.getViewMatrix();
		const glm::mat4 viewProjection = vulkanCorrection * glm::perspective(glm::radians(40.0f), width / float(height), 0.01f, 50.0f) * viewMatrix;
		const glm::mat4 shadowMatrix = vulkanCorrection * glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 3.0f) * glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const float lightRadius = 5.0f;
		const float lightLuminousPower = 700.0f;
		const glm::vec3 lightColor = glm::vec3(255.0f, 206.0f, 166.0f) / 255.0f;
		const glm::vec3 lightIntensity = lightColor * lightLuminousPower * (1.0f / (4.0f * glm::pi<float>()));

		renderer.render(viewProjection, shadowMatrix, glm::vec4(lightPos, lightRadius), glm::vec4(lightIntensity, 1.0f / (lightRadius * lightRadius)), glm::vec4(camera.getPosition(), 0.0f));

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