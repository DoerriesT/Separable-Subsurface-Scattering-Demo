#include <cstdlib>
#include "window/Window.h"
#include "input/UserInput.h"
#include "utility/Timer.h"
#include "vulkan/Renderer.h"
#include "input/ArcBallCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

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

	float lightTheta = 60.0f;
	const float lightRadius = 5.0f;
	const float lightLuminousPower = 700.0f;
	const glm::vec3 lightColor = glm::vec3(255.0f, 206.0f, 166.0f) / 255.0f;
	const glm::vec3 lightIntensity = lightColor * lightLuminousPower * (1.0f / (4.0f * glm::pi<float>()));

	bool subsurfaceScatteringEnabled = true;
	bool showGui = true;

	while (!window.shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		window.pollEvents();
		userInput.input();
		camera.update(userInput.getMousePosDelta() * (userInput.isMouseButtonPressed(InputMouse::BUTTON_RIGHT) ? 1.0f : 0.0f), userInput.getScrollOffset().y);

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Subsurface Scattering Demo!");
		ImGui::Checkbox("Subsurface Scattering", &subsurfaceScatteringEnabled);
		ImGui::SliderFloat("Light Angle", &lightTheta, 0.0f, 360.0f);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("Subsurface Scattering Time %.3f ms", renderer.getSSSEffectTiming());
		ImGui::End();

		ImGui::Render();

		float lightThetaRadians = glm::radians(lightTheta);
		glm::vec3 lightPos(glm::cos(lightThetaRadians), 0.2f, glm::sin(lightThetaRadians));

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
		
		renderer.render(viewProjection, shadowMatrix, glm::vec4(lightPos, lightRadius), glm::vec4(lightIntensity, 1.0f / (lightRadius * lightRadius)), glm::vec4(camera.getPosition(), 0.0f), subsurfaceScatteringEnabled);

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