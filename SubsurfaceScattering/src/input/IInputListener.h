#pragma once
#include "InputTokens.h"

namespace sss
{
	class ICharListener
	{
	public:
		using Codepoint = unsigned int;
		virtual void onChar(Codepoint key) = 0;
	};

	class IKeyListener
	{
	public:
		virtual void onKey(InputKey key, InputAction action) = 0;
	};

	class IMouseButtonListener
	{
	public:
		virtual void onMouseButton(InputMouse mouseButton, InputAction action) = 0;
	};

	class IMouseMoveListener
	{
	public:
		virtual void onMouseMove(double x, double y) = 0;
	};

	class IScrollListener
	{
	public:
		virtual void onMouseScroll(double xOffset, double yOffset) = 0;
	};

	class IInputListener
		: public ICharListener,
		public IKeyListener,
		public IMouseButtonListener,
		public IMouseMoveListener,
		public IScrollListener
	{

	};
}