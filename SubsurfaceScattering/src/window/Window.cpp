#include "Window.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "utility/Utility.h"
#include "utility/ContainerUtility.h"
#include "Input/IInputListener.h"

void sss::curserPosCallback(GLFWwindow *window, double xPos, double yPos);
void sss::scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
void sss::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void sss::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
void sss::charCallback(GLFWwindow *window, unsigned int codepoint);

sss::Window::Window(unsigned int width, unsigned int height, const char *title)
	:m_windowHandle(),
	m_width(width),
	m_height(height),
	m_title(title)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);

	if (!m_windowHandle)
	{
		glfwTerminate();
		util::fatalExit("Failed to create GLFW window", EXIT_FAILURE);
		return;
	}

	glfwSetCursorPosCallback(m_windowHandle, curserPosCallback);
	glfwSetScrollCallback(m_windowHandle, scrollCallback);
	glfwSetMouseButtonCallback(m_windowHandle, mouseButtonCallback);
	glfwSetKeyCallback(m_windowHandle, keyCallback);
	glfwSetCharCallback(m_windowHandle, charCallback);

	glfwSetWindowUserPointer(m_windowHandle, this);
}

sss::Window::~Window()
{
	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();
}

void sss::Window::pollEvents() const
{
	glfwPollEvents();
}

void *sss::Window::getWindowHandle() const
{
	return static_cast<void *>(m_windowHandle);
}

unsigned int sss::Window::getWidth() const
{
	return m_width;
}

unsigned int sss::Window::getHeight() const
{
	return m_height;
}

bool sss::Window::shouldClose() const
{
	return glfwWindowShouldClose(m_windowHandle);
}

void sss::Window::grabMouse(bool grabMouse)
{
	if (grabMouse)
	{
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	else
	{
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

void sss::Window::setTitle(const std::string &title)
{
	m_title = title;
	glfwSetWindowTitle(m_windowHandle, m_title.c_str());
}

void sss::Window::addInputListener(IInputListener *listener)
{
	m_inputListeners.push_back(listener);
}

void sss::Window::removeInputListener(IInputListener *listener)
{
	util::remove(m_inputListeners, listener);
}

// callback functions

void sss::curserPosCallback(GLFWwindow *window, double xPos, double yPos)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseMove(xPos, yPos);
	}
}

void sss::scrollCallback(GLFWwindow *window, double xOffset, double yOffset)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseScroll(xOffset, yOffset);
	}
}

void sss::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseButton(static_cast<InputMouse>(button), static_cast<InputAction>(action));
	}
}

void sss::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onKey(static_cast<InputKey>(key), static_cast<InputAction>(action));
	}
}

void sss::charCallback(GLFWwindow *window, unsigned int codepoint)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onChar(codepoint);
	}
}