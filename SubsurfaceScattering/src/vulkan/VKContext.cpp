#include "VKContext.h"
#include <iostream>
#include <set>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "utility/Utility.h"

bool g_vulkanDebugCallBackEnabled = true;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "validation layer:\n"
		<< "Message ID Name: " << pCallbackData->pMessageIdName << "\n"
		<< "Message ID Number: " << pCallbackData->messageIdNumber << "\n"
		<< "Message: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

sss::vulkan::VKContext::VKContext(void *windowHandle)
{
	if (volkInitialize() != VK_SUCCESS)
	{
		util::fatalExit("Failed to initialize volk!", EXIT_FAILURE);
	}

	// create instance
	{
		VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.pApplicationName = "Vulkan";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// extensions
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + static_cast<size_t>(glfwExtensionCount));

		if (g_vulkanDebugCallBackEnabled)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		// make sure all requested extensions are available
		{
			uint32_t instanceExtensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
			std::vector<VkExtensionProperties> extensionProperties(instanceExtensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, extensionProperties.data());

			for (auto &requestedExtension : extensions)
			{
				bool found = false;
				for (auto &presentExtension : extensionProperties)
				{
					if (strcmp(requestedExtension, presentExtension.extensionName) == 0)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					util::fatalExit(("Requested extension not present! " + std::string(requestedExtension)).c_str(), EXIT_FAILURE);
				}
			}
		}

		VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create instance!", EXIT_FAILURE);
		}
	}

	volkLoadInstance(m_instance);

	// create debug callback
	if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;

		if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugUtilsMessenger) != VK_SUCCESS)
		{
			util::fatalExit("Failed to set up debug callback!", EXIT_FAILURE);
		}
	}

	// create window surface
	{
		if (glfwCreateWindowSurface(m_instance, static_cast<GLFWwindow *>(windowHandle), nullptr, &m_surface) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create window surface!", EXIT_FAILURE);
		}
	}

	const char *const deviceExtensions[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	// pick physical device
	{
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);

		if (physicalDeviceCount == 0)
		{
			util::fatalExit("Failed to find GPUs with Vulkan support!", EXIT_FAILURE);
		}

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());

		for (const auto& physicalDevice : physicalDevices)
		{
			// find queue indices
			int graphicsFamilyIndex = -1;
			{
				uint32_t queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

				std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

				// find graphics queue
				for (uint32_t i = 0; i < queueFamilies.size(); ++i)
				{
					// query present support
					VkBool32 presentable = VK_FALSE;
					vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &presentable);

					auto &queueFamily = queueFamilies[i];

					if (queueFamily.queueCount > 0 
						&& (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
						&& presentable)
					{
						graphicsFamilyIndex = i;
					}
				}

				if (graphicsFamilyIndex == -1)
				{
					util::fatalExit("Failed to find suitable graphics queue with present support!", EXIT_FAILURE);
				}
			}

			// test if all required extensions are supported by this physical device
			bool extensionsSupported = false;
			{
				uint32_t extensionCount = 0;
				vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

				std::vector<VkExtensionProperties> availableExtensions(extensionCount);
				vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

				std::set<std::string> requiredExtensions(std::begin(deviceExtensions), std::end(deviceExtensions));

				for (const auto& extension : availableExtensions)
				{
					requiredExtensions.erase(extension.extensionName);
				}

				extensionsSupported = requiredExtensions.empty();
			}

			// test if the device supports a swapchain
			bool swapChainAdequate = false;
			if (extensionsSupported)
			{
				uint32_t formatCount = 0;
				vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);

				swapChainAdequate = formatCount && presentModeCount;
			}

			VkPhysicalDeviceFeatures supportedFeatures;
			vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

			if (graphicsFamilyIndex >= 0
				&& extensionsSupported
				&& swapChainAdequate
				&& supportedFeatures.samplerAnisotropy
				&& supportedFeatures.textureCompressionBC
				&& supportedFeatures.shaderSampledImageArrayDynamicIndexing)
			{
				m_physicalDevice = physicalDevice;
				m_graphicsQueueFamilyIndex = static_cast<uint32_t>(graphicsFamilyIndex);
				vkGetPhysicalDeviceProperties(physicalDevice, &m_properties);
				m_features = supportedFeatures;
				break;
			}
		}

		if (m_physicalDevice == VK_NULL_HANDLE)
		{
			util::fatalExit("Failed to find a suitable GPU!", EXIT_FAILURE);
		}
	}

	// create logical device and retrieve queues
	{

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.textureCompressionBC = VK_TRUE;
		deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

		m_enabledFeatures = deviceFeatures;

		VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		createInfo.queueCreateInfoCount = 1;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(sizeof(deviceExtensions) / sizeof(deviceExtensions[0]));
		createInfo.ppEnabledExtensionNames = deviceExtensions;

		if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create logical device!", EXIT_FAILURE);
		}

		vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	}

	volkLoadDevice(m_device);

	// create command pools
	{
		VkCommandPoolCreateInfo graphicsPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		graphicsPoolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
		graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_device, &graphicsPoolInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create graphics command pool!", EXIT_FAILURE);
		}
	}
}

sss::vulkan::VKContext::~VKContext()
{
	vkDestroyCommandPool(m_device, m_graphicsCommandPool, nullptr);
	vkDestroyDevice(m_device, nullptr);

	if (g_vulkanDebugCallBackEnabled)
	{
		if (auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"))
		{
			func(m_instance, m_debugUtilsMessenger, nullptr);
		}
	}

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

VkInstance sss::vulkan::VKContext::getInstance() const
{
	return m_instance;
}

VkDevice sss::vulkan::VKContext::getDevice() const
{
	return m_device;
}

VkPhysicalDevice sss::vulkan::VKContext::getPhysicalDevice() const
{
	return m_physicalDevice;
}

VkPhysicalDeviceFeatures sss::vulkan::VKContext::getDeviceFeatures() const
{
	return m_features;
}

VkPhysicalDeviceFeatures sss::vulkan::VKContext::getEnabledDeviceFeatures() const
{
	return m_enabledFeatures;
}

VkPhysicalDeviceProperties sss::vulkan::VKContext::getDeviceProperties() const
{
	return m_properties;
}

VkQueue sss::vulkan::VKContext::getGraphicsQueue() const
{
	return m_graphicsQueue;
}

VkCommandPool sss::vulkan::VKContext::getGraphicsCommandPool() const
{
	return m_graphicsCommandPool;
}

VkSurfaceKHR sss::vulkan::VKContext::getSurface() const
{
	return m_surface;
}

uint32_t sss::vulkan::VKContext::getGraphicsQueueFamilyIndex() const
{
	return m_graphicsQueueFamilyIndex;
}
