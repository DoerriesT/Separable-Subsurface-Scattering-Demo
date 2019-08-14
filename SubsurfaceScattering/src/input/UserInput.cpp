#include "UserInput.h"
#include "utility/ContainerUtility.h"

sss::UserInput::UserInput()
{
}

void sss::UserInput::input()
{
	if (!m_scrolled)
	{
		m_scrollOffset = glm::vec2();
	}
	else
	{
		m_scrolled = false;
	}
	
	m_mousePosDelta = (m_mousePos - m_previousMousePos);
	m_previousMousePos = m_mousePos;
}

glm::vec2 sss::UserInput::getPreviousMousePos() const
{
	return m_previousMousePos;
}

glm::vec2 sss::UserInput::getCurrentMousePos() const
{
	return m_mousePos;
}

glm::vec2 sss::UserInput::getMousePosDelta() const
{
	return m_mousePosDelta;
}

glm::vec2 sss::UserInput::getScrollOffset() const
{
	return m_scrollOffset;
}

bool sss::UserInput::isKeyPressed(InputKey key, bool ignoreRepeated) const
{
	size_t pos = static_cast<size_t>(key);
	return m_pressedKeys[pos] && (!ignoreRepeated || !m_repeatedKeys[pos]);
}

bool sss::UserInput::isMouseButtonPressed(InputMouse mouseButton) const
{
	return m_pressedMouseButtons[static_cast<size_t>(mouseButton)];
}

void sss::UserInput::addKeyListener(IKeyListener *listener)
{
	m_keyListeners.push_back(listener);
}

void sss::UserInput::removeKeyListener(IKeyListener *listener)
{
	util::remove(m_keyListeners, listener);
}

void sss::UserInput::addCharListener(ICharListener *listener)
{
	m_charListeners.push_back(listener);
}

void sss::UserInput::removeCharListener(ICharListener *listener)
{
	util::remove(m_charListeners, listener);
}

void sss::UserInput::addScrollListener(IScrollListener *listener)
{
	m_scrollListeners.push_back(listener);
}

void sss::UserInput::removeScrollListener(IScrollListener *listener)
{
	util::remove(m_scrollListeners, listener);
}

void sss::UserInput::addMouseButtonListener(IMouseButtonListener *listener)
{
	m_mouseButtonlisteners.push_back(listener);
}

void sss::UserInput::removeMouseButtonListener(IMouseButtonListener *listener)
{
	util::remove(m_mouseButtonlisteners, listener);
}

void sss::UserInput::onKey(InputKey key, InputAction action)
{
	for (IKeyListener *listener : m_keyListeners)
	{
		listener->onKey(key, action);
	}

	switch (action)
	{
	case InputAction::RELEASE:
		m_pressedKeys.set(static_cast<size_t>(key), false);
		m_repeatedKeys.set(static_cast<size_t>(key), false);
		break;
	case InputAction::PRESS:
		m_pressedKeys.set(static_cast<size_t>(key), true);
		break;
	case InputAction::REPEAT:
		m_repeatedKeys.set(static_cast<size_t>(key), true);
		break;
	default:
		break;
	}
}

void sss::UserInput::onChar(Codepoint charKey)
{
	for (ICharListener *listener : m_charListeners)
	{
		listener->onChar(charKey);
	}
}

void sss::UserInput::onMouseButton(InputMouse mouseButton, InputAction action)
{
	for (IMouseButtonListener *listener : m_mouseButtonlisteners)
	{
		listener->onMouseButton(mouseButton, action);
	}

	if (action == InputAction::RELEASE)
	{
		m_pressedMouseButtons.set(static_cast<size_t>(mouseButton), false);
	}
	else if (action == InputAction::PRESS)
	{
		m_pressedMouseButtons.set(static_cast<size_t>(mouseButton), true);
	}
}

void sss::UserInput::onMouseMove(double x, double y)
{
	m_mousePos.x = static_cast<float>(x);
	m_mousePos.y = static_cast<float>(y);
}

void sss::UserInput::onMouseScroll(double xOffset, double yOffset)
{
	for (IScrollListener *listener : m_scrollListeners)
	{
		listener->onMouseScroll(xOffset, yOffset);
	}

	m_scrollOffset.x = static_cast<float>(xOffset);
	m_scrollOffset.y = static_cast<float>(yOffset);

	m_scrolled = true;
}
