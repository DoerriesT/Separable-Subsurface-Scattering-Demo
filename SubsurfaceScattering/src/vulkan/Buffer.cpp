#include "Buffer.h"
#include "VKUtility.h"
#include "utility/Utility.h"

sss::vulkan::Buffer::Buffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkBufferCreateInfo & createInfo, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags)
	:m_physicalDevice(physicalDevice),
	m_device(device),
	m_size(createInfo.size),
	m_mappedPtr()
{
	if (vkutil::createBuffer(m_physicalDevice, m_device, createInfo, requiredFlags, preferredFlags, m_buffer, m_memory) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create buffer!", EXIT_FAILURE);
	}
}

sss::vulkan::Buffer::~Buffer()
{
	vkDestroyBuffer(m_device, m_buffer, nullptr);
	vkFreeMemory(m_device, m_memory, nullptr);
}

VkBuffer sss::vulkan::Buffer::getBuffer() const
{
	return m_buffer;
}

VkDeviceMemory sss::vulkan::Buffer::getMemory() const
{
	return m_memory;
}

size_t sss::vulkan::Buffer::getSize() const
{
	return m_size;
}

uint8_t *sss::vulkan::Buffer::map()
{
	if (!m_mappedPtr)
	{
		if (vkMapMemory(m_device, m_memory, 0, m_size, 0, (void **)&m_mappedPtr) != VK_SUCCESS)
		{
			util::fatalExit("Failed to map constant buffer memory!", EXIT_FAILURE);
		}
	}
	return m_mappedPtr;
}

void sss::vulkan::Buffer::unmap()
{
	if (m_mappedPtr)
	{
		vkUnmapMemory(m_device, m_memory);
		m_mappedPtr = nullptr;
	}
}
