#pragma once
#include "volk.h"

namespace sss
{
	namespace vulkan
	{
		namespace vkutil
		{
			VkResult findMemoryTypeIndex(VkPhysicalDeviceMemoryProperties memoryProperties, uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties, VkMemoryPropertyFlags preferredProperties, uint32_t &memoryTypeIndex);
			VkResult createImage(VkPhysicalDevice physicalDevice, VkDevice device, VkImageCreateInfo createInfo, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, VkImage &image, VkDeviceMemory &memory);
			VkResult create2dImage(VkPhysicalDevice physicalDevice, VkDevice device, VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, VkImage &image, VkDeviceMemory &memory);
		}
	}
}