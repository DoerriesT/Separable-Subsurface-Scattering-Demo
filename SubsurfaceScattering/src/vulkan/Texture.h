#pragma once
#include "volk.h"
#include <memory>

namespace sss
{
	namespace vulkan
	{
		class Texture
		{
		public:
			static std::shared_ptr<Texture> load(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool, const char *path, bool cube = false);
			explicit Texture() = default;
			Texture(const Texture &) = delete;
			Texture(const Texture &&) = delete;
			Texture &operator= (const Texture &) = delete;
			Texture &operator= (const Texture &&) = delete;
			~Texture();
			VkImageView getView() const;
			VkImage getImage() const;
			VkImageType getType() const;
			VkFormat getFormat() const;
			uint32_t getWidth() const;
			uint32_t getHeight() const;
			uint32_t getDepth() const;
			uint32_t getLevels() const;
			uint32_t getLayers() const;

		private:
			VkDevice m_device;
			VkImageView m_view;
			VkImage m_image;
			VkDeviceMemory m_deviceMemory;
			VkImageType m_imageType;
			VkFormat m_format;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_depth;
			uint32_t m_mipLevels;
			uint32_t m_arrayLayers;
		};
	}
}