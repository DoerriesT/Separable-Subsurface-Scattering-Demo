#pragma once
#include "vulkan/volk.h"
#include <vector>

namespace sss
{
	namespace vulkan
	{
		class VKContext
		{
		public:
			explicit VKContext(void *windowHandle);
			VKContext(const VKContext &) = delete;
			VKContext(const VKContext &&) = delete;
			VKContext &operator= (const VKContext &) = delete;
			VKContext &operator= (const VKContext &&) = delete;
			~VKContext();
			VkInstance getInstance() const;
			VkDevice getDevice() const;
			VkPhysicalDevice getPhysicalDevice() const;
			VkPhysicalDeviceFeatures getDeviceFeatures() const;
			VkPhysicalDeviceFeatures getEnabledDeviceFeatures() const;
			VkPhysicalDeviceProperties getDeviceProperties() const;
			VkQueue getGraphicsQueue() const;
			VkCommandPool getGraphicsCommandPool() const;
			VkSurfaceKHR getSurface() const;
			uint32_t getGraphicsQueueFamilyIndex() const;

		private:
			VkInstance m_instance;
			VkDevice m_device;
			VkPhysicalDevice m_physicalDevice;
			VkPhysicalDeviceFeatures m_features;
			VkPhysicalDeviceFeatures m_enabledFeatures;
			VkPhysicalDeviceProperties m_properties;
			VkQueue m_graphicsQueue;
			uint32_t m_graphicsQueueFamilyIndex;
			VkCommandPool m_graphicsCommandPool;
			VkSurfaceKHR m_surface;
			VkDebugUtilsMessengerEXT m_debugUtilsMessenger;
		};
	}
}