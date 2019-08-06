#include "Timer.h"
#include <GLFW/glfw3.h>

uint64_t sss::util::Timer::s_ticksPerSecond = 0;

sss::util::Timer::Timer(double timeScale)
	:m_timeScale(timeScale),
	m_paused(false)
{
	if (s_ticksPerSecond == 0)
	{
		s_ticksPerSecond = glfwGetTimerFrequency();
	}

	start();
}

void sss::util::Timer::update()
{
	if (!m_paused)
	{
		m_previousElapsedTicks = m_elapsedTicks;
		m_elapsedTicks = glfwGetTimerValue() - m_startTick;
	}
}

void sss::util::Timer::start()
{
	m_paused = false;
	m_startTick = glfwGetTimerValue();
	m_elapsedTicks = 0;
	m_previousElapsedTicks = 0;
}

void sss::util::Timer::pause()
{
	m_paused = true;
}

void sss::util::Timer::reset()
{
	m_startTick = glfwGetTimerValue();
	m_elapsedTicks = 0;
	m_previousElapsedTicks = 0;
}

bool sss::util::Timer::isPaused() const
{
	return m_paused;
}

uint64_t sss::util::Timer::getTickFrequency() const
{
	return s_ticksPerSecond;
}

uint64_t sss::util::Timer::getElapsedTicks() const
{
	return m_elapsedTicks;
}

uint64_t sss::util::Timer::getTickDelta() const
{
	return m_elapsedTicks - m_previousElapsedTicks;
}

double sss::util::Timer::getTime() const
{
	return (m_elapsedTicks / static_cast<double>(s_ticksPerSecond)) * m_timeScale;
}

double sss::util::Timer::getTimeDelta() const
{
	return ((m_elapsedTicks - m_previousElapsedTicks) / static_cast<double>(s_ticksPerSecond)) * m_timeScale;
}
