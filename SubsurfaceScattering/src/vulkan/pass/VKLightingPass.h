#pragma once
#include "vulkan/volk.h"

namespace sss
{
	namespace vulkan
	{
		class VKPipelineCache;
		struct VKRenderPassDescription;

		namespace VKLightingPass
		{
			struct Data
			{
				uint32_t m_width;
				uint32_t m_height;
			};

			void execute(VkCommandBuffer cmdBuf, VKPipelineCache &pipelineCache, const VKRenderPassDescription &renderPassDesc, VkRenderPass renderPass, const Data &data);
		};
	}
}