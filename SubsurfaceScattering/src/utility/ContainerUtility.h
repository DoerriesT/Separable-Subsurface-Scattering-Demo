#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset>
#include <algorithm>

namespace sss
{
	namespace util
	{
		template<typename T>
		inline bool find(const std::vector<T> &v, const T &item, int &position)
		{
			auto it = std::find(v.cbegin(), v.cend(), item);

			if (it < v.cend())
			{
				position = (int)(it - v.cbegin());
				return true;
			}
			return false;
		}

		template<typename T>
		inline bool contains(const std::vector<T> &v, const T &item)
		{
			return std::find(v.cbegin(), v.cend(), item) != v.cend();
		}

		template<typename Key, typename Value>
		inline bool contains(const std::map<Key, Value> &m, const Key &item)
		{
			return m.find(item) != m.cend();
		}

		template<typename Key, typename Value>
		inline bool contains(const std::unordered_map<Key, Value> &m, const Key &item)
		{
			return m.find(item) != m.cend();
		}

		template<typename T>
		inline bool contains(const std::set<T> &s, const T &item)
		{
			return s.find(item) != s.cend();
		}

		template<typename T>
		inline bool contains(const std::unordered_set<T> &set, const T &item)
		{
			return set.find(item) != set.cend();
		}

		template<typename T>
		inline void remove(std::vector<T> &v, const T &item)
		{
			v.erase(std::remove(v.begin(), v.end(), item), v.end());
		}

		template<typename Key, typename Value>
		inline void remove(std::map<Key, Value> &m, const Key &item)
		{
			m.erase(item);
		}

		template<typename Key, typename Value>
		inline void remove(std::unordered_map<Key, Value> &m, const Key &item)
		{
			m.erase(item);
		}

		template<typename T>
		inline void remove(std::set<T> &s, const T &item)
		{
			s.erase(item);
		}

		template<typename T>
		inline void remove(std::unordered_set<T> &s, const T &item)
		{
			s.erase(item);
		}

		template<typename T>
		inline void quickRemove(std::vector<T> &v, const T &item)
		{
			auto it = std::find(v.begin(), v.end(), item);
			if (it != v.end())
			{
				std::swap(v.back(), *it);
				v.erase(--v.end());
			}
		}

		template<size_t BITS>
		inline size_t findNextSetBit(const std::bitset<BITS> &bits, size_t startPos)
		{
			size_t nextSetBit = startPos;

			while (++startPos < BITS)
			{
				if (bits[startPos])
				{
					nextSetBit = startPos;
					break;
				}
			}

			return nextSetBit;
		}

		template<size_t BITS>
		inline size_t findPreviousSetBit(const std::bitset<BITS> &bits, size_t startPos)
		{
			size_t previousSetBit = startPos;

			while (startPos-- > 0)
			{
				if (bits[startPos])
				{
					previousSetBit = startPos;
					break;
				}
			}

			return previousSetBit;
		}
	}
}