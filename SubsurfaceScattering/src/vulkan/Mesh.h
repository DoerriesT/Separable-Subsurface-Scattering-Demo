#pragma once
#include "volk.h"
#include <memory>

namespace sss
{
	namespace vulkan
	{
		class Mesh
		{
		public:
			static std::shared_ptr<Mesh> load(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool, const char *path);
			Mesh() = default;
			Mesh(const Mesh &) = delete;
			Mesh(const Mesh &&) = delete;
			Mesh &operator= (const Mesh &) = delete;
			Mesh &operator= (const Mesh &&) = delete;
			~Mesh();
			uint32_t getVertexCount() const;
			uint32_t getIndexCount() const;
			VkBuffer getVertexBuffer() const;
			VkBuffer getIndexBuffer() const;

		private:
			VkDevice m_device;
			uint32_t m_vertexCount;
			uint32_t m_indexCount;
			VkBuffer m_vertexBuffer;
			VkBuffer m_indexBuffer;
			VkDeviceMemory m_vertexBufferMemory;
			VkDeviceMemory m_indexBufferMemory;
		};
	}
}