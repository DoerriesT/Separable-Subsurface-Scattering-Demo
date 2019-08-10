#include "ShaderModule.h"
#include "utility/Utility.h"

sss::vulkan::ShaderModule::ShaderModule(VkDevice device, const char *path)
	:m_device(device)
{
	std::vector<char> code = util::readBinaryFile(path);
	VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &m_module) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create shader module!", EXIT_FAILURE);
	}
}

sss::vulkan::ShaderModule::~ShaderModule()
{
	vkDestroyShaderModule(m_device, m_module, nullptr);
}

sss::vulkan::ShaderModule::operator VkShaderModule() const
{
	return m_module;
}
