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
	Window window(width, height, "Subsurface Scattering Demo");
	UserInput userInput;

	window.addInputListener(&userInput);

	// update width/height with actual window resolution
	width = window.getWidth();
	height = window.getHeight();

	// create list of supported window resolutions and a string representation for gui combo box
	auto supportedResolutions = window.getSupportedResolutions();
	int currentResolutionIndex = -1;
	std::vector<std::string> resolutionStrings;
	for (size_t i = 0; i < supportedResolutions.size(); ++i)
	{
		const auto &res = supportedResolutions[i];
		resolutionStrings.push_back(std::to_string(res.first) + "x" + std::to_string(res.second));
		if (res.first == width && res.second == height)
		{
			currentResolutionIndex = static_cast<int>(i);
		}
	}
	assert(currentResolutionIndex != -1);

	vulkan::Renderer renderer(window.getWindowHandle(), width, height);

	ArcBallCamera camera(glm::vec3(0.0f, 0.25f, 0.0f), 1.0f);

	const float lightRadius = 5.0f;
	const float lightLuminousPower = 700.0f;
	const glm::vec3 lightColor = glm::vec3(255.0f, 206.0f, 166.0f) / 255.0f;
	const glm::vec3 lightIntensity = lightColor * lightLuminousPower * (1.0f / (4.0f * glm::pi<float>()));

	bool subsurfaceScatteringEnabled = true;
	float sssWidth = 10.0f;
	bool taaEnabled = true;
	float lightTheta = 60.0f;

	glm::vec2 mouseHistory(0.0f);
	float scrollHistory = 0.0f;

	while (!window.shouldClose())
	{
		window.pollEvents();
		userInput.input();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// disable mouse and scrolling as camera input if gui is hovered
		const glm::vec2 mouseDelta = userInput.getMousePosDelta() * ((userInput.isMouseButtonPressed(InputMouse::BUTTON_RIGHT) && !ImGui::IsAnyItemHovered()) ? 1.0f : 0.0f);
		const float scrollDelta = userInput.getScrollOffset().y * (!ImGui::IsAnyItemHovered() ? 1.0f : 0.0f);

		// smooth mouse input
		mouseHistory = glm::mix(mouseHistory, mouseDelta, ImGui::GetIO().DeltaTime / (ImGui::GetIO().DeltaTime + 0.05f));
		scrollHistory = glm::mix(scrollHistory, scrollDelta, ImGui::GetIO().DeltaTime / (ImGui::GetIO().DeltaTime + 0.05f));

		// update camera
		camera.update(mouseHistory, scrollHistory);

		// gui window
		ImGui::Begin("Subsurface Scattering Demo");

		// resolution combo box
		{
			int selectedIndex = currentResolutionIndex;
			struct FuncHolder { static bool ItemGetter(void* data, int idx, const char** out_str) { *out_str = ((std::string *)data)[idx].c_str(); return true; } };
			if (ImGui::Combo("Window Resolution", &selectedIndex, &FuncHolder::ItemGetter, resolutionStrings.data(), static_cast<int>(resolutionStrings.size())) && selectedIndex != currentResolutionIndex)
			{
				currentResolutionIndex = selectedIndex;
				window.resize(supportedResolutions[currentResolutionIndex].first, supportedResolutions[currentResolutionIndex].second);
				width = window.getWidth();
				height = window.getHeight();
				renderer.resize(width, height);
			}
		}

		ImGui::Checkbox("Subsurface Scattering", &subsurfaceScatteringEnabled);
		ImGui::SliderFloat("Scattering Radius (mm)", &sssWidth, 1.0f, 40.0f);
		ImGui::Checkbox("Temporal AA", &taaEnabled);
		ImGui::SliderFloat("Light Angle", &lightTheta, 0.0f, 360.0f);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("Subsurface Scattering Time %.3f ms", renderer.getSSSEffectTiming());
		ImGui::End();

		ImGui::Render();

		// calculate light position
		const float lightThetaRadians = glm::radians(lightTheta);
		const glm::vec3 lightPos(glm::cos(lightThetaRadians), 0.2f, glm::sin(lightThetaRadians));

		// calculate view, projection and shadow matrix
		const glm::mat4 vulkanCorrection =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 1.0f }
		};

		const glm::mat4 viewMatrix = camera.getViewMatrix();
		const glm::mat4 viewProjection = vulkanCorrection * glm::perspective(glm::radians(40.0f), width / float(height), 0.01f, 50.0f) * viewMatrix;
		const glm::mat4 shadowMatrix = vulkanCorrection * glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 3.0f) * glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		
		renderer.render(viewProjection, 
			shadowMatrix, 
			glm::vec4(lightPos, lightRadius), 
			glm::vec4(lightIntensity, 1.0f / (lightRadius * lightRadius)), 
			glm::vec4(camera.getPosition(), 0.0f), 
			subsurfaceScatteringEnabled,
			sssWidth * 0.001f,
			taaEnabled);
	}

	return EXIT_SUCCESS;
}