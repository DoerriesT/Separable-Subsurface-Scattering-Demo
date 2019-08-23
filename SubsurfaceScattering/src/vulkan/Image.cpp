#include "Image.h"
#include "VKUtility.h"
#include "utility/Utility.h"

sss::vulkan::Image::Image(VkPhysicalDevice physicalDevice, VkDevice device, const VkImageCreateInfo &createInfo,
	VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, VkImageViewType viewType, const VkImageSubresourceRange &subresourceRange)
	:m_physicalDevice(physicalDevice),
	m_device(device)
{
	if (vkutil::createImage(m_physicalDevice, m_device, createInfo, requiredFlags, preferredFlags, m_image, m_memory) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create image!", EXIT_FAILURE);
	}

	VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewCreateInfo.viewType = viewType;
	viewCreateInfo.image = m_image;
	viewCreateInfo.format = createInfo.format;
	viewCreateInfo.subresourceRange = subresourceRange;
	
	if (vkCreateImageView(m_device, &viewCreateInfo, nullptr, &m_view) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create image view!", EXIT_FAILURE);
	}
}

sss::vulkan::Image::~Image()
{
	vkDestroyImageView(m_device, m_view, nullptr);
	vkDestroyImage(m_device, m_image, nullptr);
	vkFreeMemory(m_device, m_memory, nullptr);
}

const VkImage &sss::vulkan::Image::getImage() const
{
	return m_image;
}

const VkImageView &sss::vulkan::Image::getView() const
{
	return m_view;
}

const VkDeviceMemory &sss::vulkan::Image::getMemory() const
{
	return m_memory;
}
