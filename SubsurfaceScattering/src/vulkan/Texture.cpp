#include "Texture.h"
#include "utility/ContainerUtility.h"
#include "utility/Utility.h"
#include "VKUtility.h"
#include <gli/texture.hpp>
#include <gli/load.hpp>

std::shared_ptr<sss::vulkan::Texture> sss::vulkan::Texture::load(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool, const char *path, bool cube)
{
	std::shared_ptr<Texture> texture = std::make_shared<Texture>();

	// load texture
	{
		gli::texture gliTex(gli::load(path));
		{
			if (gliTex.empty())
			{
				util::fatalExit(("Failed to load texture: " + std::string(path)).c_str(), EXIT_FAILURE);
			}
		}

		// fill out info values
		texture->m_device = device;
		texture->m_imageType = VK_IMAGE_TYPE_2D;
		texture->m_format = VkFormat(gliTex.format());
		texture->m_width = static_cast<uint32_t>(gliTex.extent().x);
		texture->m_height = static_cast<uint32_t>(gliTex.extent().y);
		texture->m_depth = static_cast<uint32_t>(gliTex.extent().z);
		texture->m_mipLevels = static_cast<uint32_t>(gliTex.levels());
		texture->m_arrayLayers = cube ? static_cast<uint32_t>(gliTex.faces()) : static_cast<uint32_t>(gliTex.layers());

		// load image
		{
			VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			imageCreateInfo.flags = cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
			imageCreateInfo.imageType = texture->m_imageType;
			imageCreateInfo.format = texture->m_format;
			imageCreateInfo.extent.width = texture->m_width;
			imageCreateInfo.extent.height = texture->m_height;
			imageCreateInfo.extent.depth = texture->m_depth;
			imageCreateInfo.mipLevels = texture->m_mipLevels;
			imageCreateInfo.arrayLayers = texture->m_arrayLayers;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (vkutil::createImage(physicalDevice, device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, texture->m_image, texture->m_deviceMemory) != VK_SUCCESS)
			{
				util::fatalExit(("Failed to create texture: " + std::string(path)).c_str(), EXIT_FAILURE);
			}
		}

		VkImageSubresourceRange subresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, texture->m_mipLevels, 0, texture->m_arrayLayers };

		// load image view
		{
			VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = texture->m_image;
			viewInfo.viewType = cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = texture->m_format;
			viewInfo.subresourceRange = subresourceRange;

			if (vkCreateImageView(device, &viewInfo, nullptr, &texture->m_view) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create image view!", EXIT_FAILURE);
			}
		}

		// staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		{
			VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			createInfo.size = gliTex.size();
			createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkutil::createBuffer(physicalDevice, device, createInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, stagingBuffer, stagingBufferMemory) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create staging buffer!", EXIT_FAILURE);
			}
		}

		// write data to staging buffer
		{
			uint8_t *ptr;
			if (vkMapMemory(device, stagingBufferMemory, 0, gliTex.size(), 0, (void **)&ptr) != VK_SUCCESS)
			{
				util::fatalExit("Failed to map staging buffer memory!", EXIT_FAILURE);
			}

			memcpy(ptr, gliTex.data(), gliTex.size());

			vkUnmapMemory(device, stagingBufferMemory);
		}

		// copy from staging buffer to image
		{
			std::vector<VkBufferImageCopy> bufferCopyRegions;
			uint32_t offset = 0;

			for (uint32_t layer = 0; layer < texture->m_arrayLayers; ++layer)
			{
				for (uint32_t level = 0; level < texture->m_mipLevels; ++level)
				{
					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel = level;
					bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent.width = gliTex.extent(level).x;
					bufferCopyRegion.imageExtent.height = gliTex.extent(level).y;
					bufferCopyRegion.imageExtent.depth = gliTex.extent(level).z;
					bufferCopyRegion.bufferOffset = offset;

					bufferCopyRegions.push_back(bufferCopyRegion);

					// Increase offset into staging buffer for next level / face
					offset += gliTex.size(level);
				}
			}
			assert(offset == gliTex.size());

			// execute copy from staging buffer to image
			VkCommandBuffer copyCmd = vkutil::beginSingleTimeCommands(device, cmdPool);
			{
				VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarrier.srcAccessMask = 0;
				imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.image = texture->m_image;
				imageBarrier.subresourceRange = subresourceRange;

				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

				vkCmdCopyBufferToImage(copyCmd, stagingBuffer, texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

				imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageBarrier.dstAccessMask = 0;
				imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
			}
			vkutil::endSingleTimeCommands(device, queue, cmdPool, copyCmd);
		}

		// destroy staging buffer
		{
			vkDestroyBuffer(device, stagingBuffer, nullptr);
			vkFreeMemory(device, stagingBufferMemory, nullptr);
		}
	}

	return texture;
}

sss::vulkan::Texture::~Texture()
{
	vkDestroyImageView(m_device, m_view, nullptr);
	vkDestroyImage(m_device, m_image, nullptr);
	vkFreeMemory(m_device, m_deviceMemory, nullptr);
}

VkImageView sss::vulkan::Texture::getView() const
{
	return m_view;
}

VkImage sss::vulkan::Texture::getImage() const
{
	return m_image;
}

VkImageType sss::vulkan::Texture::getType() const
{
	return m_imageType;
}

VkFormat sss::vulkan::Texture::getFormat() const
{
	return m_format;
}

uint32_t sss::vulkan::Texture::getWidth() const
{
	return m_width;
}

uint32_t sss::vulkan::Texture::getHeight() const
{
	return m_height;
}

uint32_t sss::vulkan::Texture::getDepth() const
{
	return m_depth;
}

uint32_t sss::vulkan::Texture::getLevels() const
{
	return m_mipLevels;
}

uint32_t sss::vulkan::Texture::getLayers() const
{
	return m_arrayLayers;
}
