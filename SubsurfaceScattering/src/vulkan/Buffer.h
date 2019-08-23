#pragma once
#include "volk.h"

namespace sss
{
	namespace vulkan
	{
		class Buffer
		{
		public:
			explicit Buffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkBufferCreateInfo &createInfo, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags);
			Buffer(const Buffer &) = delete;
			Buffer(const Buffer &&) = delete;
			Buffer &operator= (const Buffer &) = delete;
			Buffer &operator= (const Buffer &&) = delete;
			~Buffer();
			VkBuffer getBuffer() const;
			VkDeviceMemory getMemory() const;
			size_t getSize() const;
			uint8_t *map();
			void unmap();

		private:
			VkPhysicalDevice m_physicalDevice;
			VkDevice m_device;
			VkDeviceSize m_size;
			uint8_t *m_mappedPtr;
			VkBuffer m_buffer;
			VkDeviceMemory m_memory;
		};
	}
}