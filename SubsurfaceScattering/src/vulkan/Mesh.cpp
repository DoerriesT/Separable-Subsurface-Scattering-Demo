#include "Mesh.h"
#include <iostream>
#include <algorithm>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/common.hpp>
#include <set>
#include <unordered_map>
#include "utility/Utility.h"
#include "VKUtility.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
};

inline bool operator==(const Vertex &lhs, const Vertex &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

template <class T>
inline void hashCombine(size_t &s, const T &v)
{
	std::hash<T> h;
	s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

struct VertexHash
{
	inline size_t operator()(const Vertex &value) const
	{
		size_t hashValue = 0;

		for (size_t i = 0; i < sizeof(Vertex); ++i)
		{
			hashCombine(hashValue, reinterpret_cast<const char *>(&value)[i]);
		}

		return hashValue;
	}
};

std::vector<std::unique_ptr<sss::vulkan::Mesh>> sss::vulkan::Mesh::load(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool, const char *path)
{
	std::vector<std::unique_ptr<sss::vulkan::Mesh>> result;

	// load scene
	tinyobj::attrib_t objAttrib;
	std::vector<tinyobj::shape_t> objShapes;
	std::vector<tinyobj::material_t> objMaterials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &warn, &err, path, nullptr, true);

	if (!warn.empty())
	{
		std::cout << warn << std::endl;
	}

	if (!err.empty())
	{
		std::cerr << err << std::endl;
	}

	if (!ret)
	{
		util::fatalExit("Failed to load file!", EXIT_FAILURE);
	}

	struct Index
	{
		uint32_t m_shapeIndex;
		uint32_t m_materialIndex;
		tinyobj::index_t m_vertexIndices[3];
	};

	std::vector<Index> unifiedIndices;

	for (size_t shapeIndex = 0; shapeIndex < objShapes.size(); ++shapeIndex)
	{
		const auto &shape = objShapes[shapeIndex];
		for (size_t meshIndex = 0; meshIndex < shape.mesh.indices.size(); meshIndex += 3)
		{
			Index unifiedIndex;
			unifiedIndex.m_shapeIndex = static_cast<uint32_t>(shapeIndex);
			unifiedIndex.m_materialIndex = shape.mesh.material_ids[meshIndex / 3];
			unifiedIndex.m_vertexIndices[0] = shape.mesh.indices[meshIndex + 0];
			unifiedIndex.m_vertexIndices[1] = shape.mesh.indices[meshIndex + 1];
			unifiedIndex.m_vertexIndices[2] = shape.mesh.indices[meshIndex + 2];

			unifiedIndices.push_back(unifiedIndex);
		}
	}

	// sort only inside same shape by material
	std::stable_sort(unifiedIndices.begin(), unifiedIndices.end(),
		[](const auto &lhs, const auto &rhs)
	{
		return lhs.m_shapeIndex < rhs.m_shapeIndex || (lhs.m_shapeIndex == rhs.m_shapeIndex && lhs.m_materialIndex < rhs.m_materialIndex);
	});

	glm::vec3 minMeshCorner = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());

	size_t fileOffset = 0;
	size_t subMeshIndex = 0;

	glm::vec3 minSubMeshCorner = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxSubMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint16_t> indices;
	std::set<uint32_t> shapeIndices;
	std::unordered_map<Vertex, uint16_t, VertexHash> vertexToIndex;

	for (size_t i = 0; i < unifiedIndices.size(); ++i)
	{
		const auto &index = unifiedIndices[i];

		// loop over face (triangle)
		for (size_t j = 0; j < 3; ++j)
		{
			Vertex vertex;
			const auto &vertexIndex = index.m_vertexIndices[j];

			vertex.position.x = objAttrib.vertices[vertexIndex.vertex_index * 3 + 0];
			vertex.position.y = objAttrib.vertices[vertexIndex.vertex_index * 3 + 1];
			vertex.position.z = objAttrib.vertices[vertexIndex.vertex_index * 3 + 2];

			vertex.normal.x = objAttrib.normals[vertexIndex.normal_index * 3 + 0];
			vertex.normal.y = objAttrib.normals[vertexIndex.normal_index * 3 + 1];
			vertex.normal.z = objAttrib.normals[vertexIndex.normal_index * 3 + 2];

			if (objAttrib.texcoords.size() > vertexIndex.texcoord_index * 2 + 1)
			{
				vertex.texCoord.x = objAttrib.texcoords[vertexIndex.texcoord_index * 2 + 0];
				vertex.texCoord.y = objAttrib.texcoords[vertexIndex.texcoord_index * 2 + 1];
			}
			else
			{
				vertex.texCoord.x = 0.0f;
				vertex.texCoord.y = 0.0f;
			}

			// find min/max corners
			minSubMeshCorner = glm::min(minSubMeshCorner, vertex.position);
			maxSubMeshCorner = glm::max(maxSubMeshCorner, vertex.position);
			minMeshCorner = glm::min(minMeshCorner, vertex.position);
			maxMeshCorner = glm::max(maxMeshCorner, vertex.position);

			// if we havent encountered this vertex before, add it to the map
			if (vertexToIndex.count(vertex) == 0)
			{
				vertexToIndex[vertex] = static_cast<uint16_t>(positions.size());

				positions.push_back(vertex.position);
				normals.push_back(vertex.normal);
				texCoords.push_back(vertex.texCoord);
			}
			indices.push_back(vertexToIndex[vertex]);
		}

		shapeIndices.insert(index.m_shapeIndex);

		// create mesh and reset data
		if ((i + 1) == unifiedIndices.size()														// last index -> we're done
			|| indices.size() >= std::numeric_limits<uint16_t>::max() - 3
			|| unifiedIndices[i + 1].m_materialIndex != index.m_materialIndex					// next material is different -> create mesh
			|| (unifiedIndices[i + 1].m_shapeIndex != index.m_shapeIndex))	// dont merge by material (keep distinct objects) and next shape is different -> create mesh
		{
			auto mesh = std::make_unique<Mesh>();
			mesh->m_device = device;
			mesh->m_indexCount = static_cast<uint32_t>(indices.size());
			mesh->m_vertexCount = static_cast<uint32_t>(positions.size());
			mesh->m_minCorner = minSubMeshCorner;
			mesh->m_maxCorner = maxSubMeshCorner;

			const VkDeviceSize vertexBufferSize = positions.size() * sizeof(glm::vec3) + normals.size() * sizeof(glm::vec3) + texCoords.size() * sizeof(glm::vec2);
			const VkDeviceSize indexBufferSize = indices.size() * sizeof(uint16_t);

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

				memcpy(ptr, positions.data(), positions.size() * sizeof(glm::vec3));
				ptr += positions.size() * sizeof(glm::vec3);
				memcpy(ptr, normals.data(), normals.size() * sizeof(glm::vec3));
				ptr += normals.size() * sizeof(glm::vec3);
				memcpy(ptr, texCoords.data(), texCoords.size() * sizeof(glm::vec2));
				ptr += texCoords.size() * sizeof(glm::vec2);
				memcpy(ptr, indices.data(), indices.size() * sizeof(uint16_t));

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
					vkCmdCopyBuffer(copyCmd,stagingBuffer, mesh->m_vertexBuffer, 1, bufferCopies);
					vkCmdCopyBuffer(copyCmd,stagingBuffer, mesh->m_indexBuffer, 1, bufferCopies + 1);
				}
				vkutil::endSingleTimeCommands(device, queue, cmdPool, copyCmd);
			}

			// destroy staging buffer
			{
				vkDestroyBuffer(device, stagingBuffer, nullptr);
				vkFreeMemory(device, stagingBufferMemory, nullptr);
			}

			result.push_back(std::move(mesh));

			// reset data
			minSubMeshCorner = glm::vec3(std::numeric_limits<float>::max());
			maxSubMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());
			positions.clear();
			normals.clear();
			texCoords.clear();
			indices.clear();
			shapeIndices.clear();
			vertexToIndex.clear();

			std::cout << "Processed SubMesh # " << subMeshIndex++ << std::endl;
		}
	}

	return result;
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

glm::vec3 sss::vulkan::Mesh::getMinCorner() const
{
	return m_minCorner;
}

glm::vec3 sss::vulkan::Mesh::getMaxCorner() const
{
	return m_maxCorner;
}
