#pragma once
#include "vulkan/volk.h"

namespace sss
{
	namespace vulkan
	{
		class ShaderModule
		{
		public:
			explicit ShaderModule(VkDevice device, const char *path);
			ShaderModule(const ShaderModule &) = delete;
			ShaderModule(const ShaderModule &&) = delete;
			ShaderModule &operator= (const ShaderModule &) = delete;
			ShaderModule &operator= (const ShaderModule &&) = delete;
			~ShaderModule();
			operator VkShaderModule() const;

		private:
			VkDevice m_device;
			VkShaderModule m_module;

		};
	}
}