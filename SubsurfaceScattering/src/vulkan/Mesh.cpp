#include "Mesh.h"
#include <fstream>
#include "utility/Utility.h"
#include "VKUtility.h"


std::shared_ptr<sss::vulkan::Mesh> sss::vulkan::Mesh::load(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool, const char *path)
{
	const std::vector<char> meshData = util::readBinaryFile(path);
	const uint8_t *meshDataPtr = reinterpret_cast<const uint8_t *>(meshData.data());

	const uint32_t vertexCount = ((uint32_t *)meshDataPtr)[0];
	const uint32_t indexCount = ((uint32_t *)meshDataPtr)[1];
	const uint32_t positionsSize = vertexCount * sizeof(float) * 3;
	const uint32_t normalsSize = vertexCount * sizeof(float) * 3;
	const uint32_t texCoordsSize = vertexCount * sizeof(float) * 2;

	auto mesh = std::make_shared<Mesh>();
	mesh->m_device = device;
	mesh->m_indexCount = indexCount;
	mesh->m_vertexCount = vertexCount;

	const VkDeviceSize vertexBufferSize = positionsSize + normalsSize + texCoordsSize;
	const VkDeviceSize indexBufferSize = indexCount * sizeof(uint32_t);

	// vertex buffer
	{
		VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		createInfo.size = vertexBufferSize;
		createInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkutil::createBuffer(physicalDevice, device, createInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, mesh->m_vertexBuffer, mesh->m_vertexBufferMemory) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create vertex buffer!", EXIT_FAILURE);
		}
	}

	// index buffer
	{
		VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		createInfo.size = indexBufferSize;
		createInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkutil::createBuffer(physicalDevice, device, createInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, mesh->m_indexBuffer, mesh->m_indexBufferMemory) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create index buffer!", EXIT_FAILURE);
		}
	}

	// staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	{
		VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		createInfo.size = vertexBufferSize + indexBufferSize;
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
		if (vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize + indexBufferSize, 0, (void **)&ptr) != VK_SUCCESS)
		{
			util::fatalExit("Failed to map staging buffer memory!", EXIT_FAILURE);
		}

		meshDataPtr += 8;
		memcpy(ptr, meshDataPtr, positionsSize);
		meshDataPtr += positionsSize;
		ptr += positionsSize;
		memcpy(ptr, meshDataPtr, normalsSize);
		meshDataPtr += normalsSize;
		ptr += normalsSize;
		memcpy(ptr, meshDataPtr, texCoordsSize);
		meshDataPtr += texCoordsSize;
		ptr += texCoordsSize;
		memcpy(ptr, meshDataPtr, indexBufferSize);

		vkUnmapMemory(device, stagingBufferMemory);
	}

	// copy from staging buffer to vertex and index buffer
	{
		VkBufferCopy bufferCopies[] =
		{
			{ 0, 0, vertexBufferSize },
			{ vertexBufferSize, 0, indexBufferSize }
		};

		VkCommandBuffer copyCmd = vkutil::beginSingleTimeCommands(device, cmdPool);
		{
			vkCmdCopyBuffer(copyCmd, stagingBuffer, mesh->m_vertexBuffer, 1, bufferCopies);
			vkCmdCopyBuffer(copyCmd, stagingBuffer, mesh->m_indexBuffer, 1, bufferCopies + 1);
		}
		vkutil::endSingleTimeCommands(device, queue, cmdPool, copyCmd);
	}

	// destroy staging buffer
	{
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	return mesh;
}

sss::vulkan::Mesh::~Mesh()
{
	vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
	vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
	vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
	vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
}

uint32_t sss::vulkan::Mesh::getVertexCount() const
{
	return m_vertexCount;
}

uint32_t sss::vulkan::Mesh::getIndexCount() const
{
	return m_indexCount;
}

VkBuffer sss::vulkan::Mesh::getVertexBuffer() const
{
	return m_vertexBuffer;
}

VkBuffer sss::vulkan::Mesh::getIndexBuffer() const
{
	return m_indexBuffer;
}