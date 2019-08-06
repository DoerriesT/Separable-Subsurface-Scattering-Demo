#pragma once
#include "VKContext.h"

namespace sss
{
	namespace vulkan
	{
		class VKRenderer
		{
		public:
			explicit VKRenderer(void *windowHandle);
			void render();

		private:
			VKContext m_context;
		};
	}
}