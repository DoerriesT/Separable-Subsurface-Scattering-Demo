#include "SwapChain.h"
#include "VKContext.h"
#include "utility/Utility.h"
#include <glm/common.hpp>

sss::vulkan::SwapChain::SwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height)
	:m_physicalDevice(physicalDevice),
	m_device(device),
	m_surface(surface)
{
	create(width, height);
}

sss::vulkan::SwapChain::~SwapChain()
{
	destroy();
}

void sss::vulkan::SwapChain::recreate(uint32_t width, uint32_t height)
{
	vkDeviceWaitIdle(m_device);
	destroy();
	create(width, height);
}

VkExtent2D sss::vulkan::SwapChain::getExtent() const
{
	return m_extent;
}

VkFormat sss::vulkan::SwapChain::getImageFormat() const
{
	return m_imageFormat;
}

sss::vulkan::SwapChain::operator VkSwapchainKHR() const
{
	return m_swapChain;
}

VkImage sss::vulkan::SwapChain::getImage(size_t index) const
{
	return m_images[index];
}

VkImageView sss::vulkan::SwapChain::getImageView(size_t index) const
{
	return m_imageViews[index];
}

size_t sss::vulkan::SwapChain::getImageCount() const
{
	return m_images.size();
}

void sss::vulkan::SwapChain::create(uint32_t width, uint32_t height)
{
	// find surface format
	VkSurfaceFormatKHR surfaceFormat;
	{
		std::vector<VkSurfaceFormatKHR> formats;
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());
		}

		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}
		else
		{
			bool foundOptimal = false;
			for (const auto& format : formats)
			{
				if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					surfaceFormat = format;
					foundOptimal = true;
					break;
				}
			}
			if (!foundOptimal)
			{
				surfaceFormat = formats[0];
			}
		}
	}

	// find present mode
	VkPresentModeKHR presentMode;
	{
		std::vector<VkPresentModeKHR> presentModes;
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());
		}

		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

		bool foundOptimal = false;
		for (const auto& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = mode;
				foundOptimal = true;
				break;
			}
			else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				bestMode = mode;
			}
		}
		if (!foundOptimal)
		{
			presentMode = bestMode;
		}
	}

	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

	// find proper extent
	VkExtent2D extent;
	{
		if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			extent = caps.currentExtent;
		}
		else
		{
			extent = { glm::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width), glm::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height) };
		}
	}

	uint32_t imageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
	{
		imageCount = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = caps.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create swap chain!", EXIT_FAILURE);
	}

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
	m_images.resize(static_cast<size_t>(imageCount));
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_images.data());

	m_imageFormat = surfaceFormat.format;
	m_extent = extent;

	m_imageViews.resize(m_images.size());


	for (size_t i = 0; i < m_images.size(); ++i)
	{
		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_imageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create swapchain image view!", EXIT_FAILURE);
		}
	}
}

void sss::vulkan::SwapChain::destroy()
{
	for (auto imageView : m_imageViews)
	{
		vkDestroyImageView(m_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}
