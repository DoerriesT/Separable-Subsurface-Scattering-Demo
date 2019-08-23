#pragma once
#include "volk.h"

namespace sss
{
	namespace vulkan
	{
		class Image
		{
		public:
			explicit Image(VkPhysicalDevice physicalDevice, VkDevice device, const VkImageCreateInfo &createInfo, 
				VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, VkImageViewType viewType, const VkImageSubresourceRange &subresourceRange);
			Image(const Image &) = delete;
			Image(const Image &&) = delete;
			Image &operator= (const Image &) = delete;
			Image &operator= (const Image &&) = delete;
			~Image();
			const VkImage &getImage() const;
			const VkImageView &getView() const;
			const VkDeviceMemory &getMemory() const;

		private:
			VkPhysicalDevice m_physicalDevice;
			VkDevice m_device;
			VkImage m_image;
			VkImageView m_view;
			VkDeviceMemory m_memory;
		};
	}
}