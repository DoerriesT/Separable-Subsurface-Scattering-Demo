#pragma once
#include <cstdint>

namespace sss
{
	namespace util
	{
		class Timer
		{
		public:
			explicit Timer(double timeScale = 1.0);
			void update();
			void start();
			void pause();
			void reset();
			bool isPaused() const;
			uint64_t getTickFrequency() const;
			uint64_t getElapsedTicks() const;
			uint64_t getTickDelta() const;
			double getTime() const;
			double getTimeDelta() const;


		private:
			static uint64_t s_ticksPerSecond;
			bool m_paused;
			double m_timeScale;
			uint64_t m_startTick;
			uint64_t m_elapsedTicks;
			uint64_t m_previousElapsedTicks;
		};
	}

}