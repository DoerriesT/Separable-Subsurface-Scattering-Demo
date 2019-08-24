#pragma once
#include <cstdint>

namespace sss
{
	namespace vulkan
	{
		struct Material
		{
			float gloss;
			float specular;
			float detailNormalScale;
			uint32_t albedo;
			uint32_t albedoTexture;
			uint32_t normalTexture;
			uint32_t glossTexture;
			uint32_t specularTexture;
			uint32_t cavityTexture;
			uint32_t detailNormalTexture;
		};
	}
}