#include "VKPipeline.h"
#include <cassert>
#include "utility/Utility.h"

sss::vulkan::VKShaderStageDescription::SpecializationInfo::SpecializationInfo()
{
	m_info.mapEntryCount = 0;
	m_info.pMapEntries = m_entries;
	m_info.pData = m_data;
	m_info.dataSize = sizeof(m_data);
}

void sss::vulkan::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, int32_t value)
{
	addEntry(constantID, &value);
}

void sss::vulkan::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, uint32_t value)
{
	addEntry(constantID, &value);
}

void sss::vulkan::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, float value)
{
	addEntry(constantID, &value);
}

const VkSpecializationInfo * sss::vulkan::VKShaderStageDescription::SpecializationInfo::getInfo() const
{
	return &m_info;
}

void sss::vulkan::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, void * value)
{
	assert(m_info.mapEntryCount < MAX_ENTRY_COUNT);
	memcpy(&m_data[m_info.mapEntryCount * 4], value, 4);
	m_entries[m_info.mapEntryCount] = { constantID, m_info.mapEntryCount * 4, 4 };
	++m_info.mapEntryCount;
}

void sss::vulkan::VKRenderPassDescription::finalize()
{
	if (!m_depthStencilAttachmentPresent)
	{
		m_depthStencilAttachment = {};
	}

	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKRenderPassDescription) - sizeof(m_hashValue); ++i)
	{
		util::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

void sss::vulkan::VKGraphicsPipelineDescription::finalize()
{
	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKGraphicsPipelineDescription); ++i)
	{
		util::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

void sss::vulkan::VKComputePipelineDescription::finalize()
{
	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKComputePipelineDescription); ++i)
	{
		util::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

size_t sss::vulkan::VKRenderPassDescriptionHash::operator()(const VKRenderPassDescription & value) const
{
	return value.m_hashValue;
}

size_t sss::vulkan::VKGraphicsPipelineDescriptionHash::operator()(const VKGraphicsPipelineDescription & value) const
{
	return value.m_hashValue;
}

size_t sss::vulkan::VKComputePipelineDescriptionHash::operator()(const VKComputePipelineDescription & value) const
{
	return value.m_hashValue;
}

size_t sss::vulkan::VKCombinedGraphicsPipelineRenderPassDescriptionHash::operator()(const VKCombinedGraphicsPipelineRenderPassDescription & value) const
{
	size_t result = value.m_graphicsPipelineDescription.m_hashValue;
	util::hashCombine(result, value.m_renderPassDescription.m_hashValue);
	util::hashCombine(result, value.m_subpassIndex);
	return result;
}
