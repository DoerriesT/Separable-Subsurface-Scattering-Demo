#pragma once
#include <vector>
#include <string>

namespace sss
{
	namespace util
	{
		std::vector<char> readBinaryFile(const char *filepath);
		void fatalExit(const char *message, int exitCode);
		std::string getFileExtension(const std::string &filepath);

		template <class T>
		inline void hashCombine(size_t &s, const T &v)
		{
			std::hash<T> h;
			s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
		}
	}
}