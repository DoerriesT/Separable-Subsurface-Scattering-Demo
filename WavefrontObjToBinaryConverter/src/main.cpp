#include <cstdlib>
#include <iostream>
#include <string>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Windows.h>
#undef min
#undef max
#undef OPAQUE

void fatalExit(const char *message, int exitCode)
{
	MessageBox(nullptr, message, nullptr, MB_OK | MB_ICONERROR);
	exit(exitCode);
}

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

int main()
{
	while (true)
	{
		std::string srcFileName;
		std::string dstFileName;

		std::cout << "Source File:" << std::endl;
		std::cin >> srcFileName;
		std::cout << "Destination File:" << std::endl;
		std::cin >> dstFileName;
		std::cout << "Invert UV y-axis? (yes = 1, no = 0)";
		bool invertTexcoordY = false;
		std::cin >> invertTexcoordY;

		// load scene
		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		std::string warn;
		std::string err;

		bool ret = tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &warn, &err, srcFileName.c_str(), nullptr, true);

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
			fatalExit("Failed to load file!", EXIT_FAILURE);
		}

		// extract data into buffers and create global index list
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> texCoords;
		std::vector<uint32_t> indices;
		std::unordered_map<Vertex, uint32_t, VertexHash> vertexToIndex;

		for (const auto &shape : objShapes)
		{
			for (const auto &index : shape.mesh.indices)
			{
				Vertex vertex;

				vertex.position.x = objAttrib.vertices[index.vertex_index * 3 + 0];
				vertex.position.y = objAttrib.vertices[index.vertex_index * 3 + 1];
				vertex.position.z = objAttrib.vertices[index.vertex_index * 3 + 2];

				vertex.normal.x = objAttrib.normals[index.normal_index * 3 + 0];
				vertex.normal.y = objAttrib.normals[index.normal_index * 3 + 1];
				vertex.normal.z = objAttrib.normals[index.normal_index * 3 + 2];

				if (objAttrib.texcoords.size() > index.texcoord_index * 2 + 1)
				{
					vertex.texCoord.x = objAttrib.texcoords[index.texcoord_index * 2 + 0];
					vertex.texCoord.y = objAttrib.texcoords[index.texcoord_index * 2 + 1];
					if (invertTexcoordY)
					{
						vertex.texCoord.y = 1.0f - vertex.texCoord.y;
					}
				}
				else
				{
					vertex.texCoord.x = 0.0f;
					vertex.texCoord.y = 0.0f;
				}

				// if we havent encountered this vertex before, add it to the map
				if (vertexToIndex.count(vertex) == 0)
				{
					vertexToIndex[vertex] = static_cast<uint32_t>(positions.size());

					positions.push_back(vertex.position);
					normals.push_back(vertex.normal);
					texCoords.push_back(vertex.texCoord);
				}
				indices.push_back(vertexToIndex[vertex]);
			}
		}

		// write all the data to file
		std::ofstream dstFile(dstFileName + ".mesh", std::ios::out | std::ios::binary | std::ios::trunc);
		const uint32_t vertexCount = static_cast<uint32_t>(positions.size());
		const uint32_t indexCount = static_cast<uint32_t>(indices.size());;

		dstFile.write((const char *)&vertexCount, sizeof(uint32_t));
		dstFile.write((const char *)&indexCount, sizeof(uint32_t));
		dstFile.write((const char *)positions.data(), positions.size() * sizeof(glm::vec3));
		dstFile.write((const char *)normals.data(), normals.size() * sizeof(glm::vec3));
		dstFile.write((const char *)texCoords.data(), texCoords.size() * sizeof(glm::vec2));
		dstFile.write((const char *)indices.data(), indices.size() * sizeof(uint32_t));
		dstFile.close();

		std::cout << "Finished processing mesh with " << positions.size() << " vertices and " << indices.size() << " indices." << std::endl;
	}

	return EXIT_SUCCESS;
}