#pragma once
#include "vulkan/volk.h"
#include <utility>

namespace sss
{
	namespace vulkan
	{
		namespace LightingPipeline
		{
			std::pair<VkPipeline, VkPipelineLayout> create(VkDevice device, VkRenderPass renderPass, uint32_t subpassIndex, uint32_t setLayoutCount, VkDescriptorSetLayout *setLayouts, bool subsurfaceScattering);
		}
	}
}