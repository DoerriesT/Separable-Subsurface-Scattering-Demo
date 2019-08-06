#pragma once
#include <vector>
#include <string>
#include "input/IInputListener.h"

struct GLFWwindow;

namespace sss
{
	class Window
	{
		friend void curserPosCallback(GLFWwindow *window, double xPos, double yPos);
		friend void scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
		friend void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
		friend void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
		friend void charCallback(GLFWwindow *window, unsigned int codepoint);
	public:
		explicit Window(unsigned int width, unsigned int height, const char *title);
		~Window();
		void pollEvents() const;
		void *getWindowHandle() const;
		unsigned int getWidth() const;
		unsigned int getHeight() const;
		bool shouldClose() const;
		void grabMouse(bool grabMouse);
		void setTitle(const std::string &title);
		void addInputListener(IInputListener *listener);
		void removeInputListener(IInputListener *listener);

	private:
		GLFWwindow *m_windowHandle;
		unsigned int m_width;
		unsigned int m_height;
		std::string m_title;
		std::vector<IInputListener*> m_inputListeners;
	};
}