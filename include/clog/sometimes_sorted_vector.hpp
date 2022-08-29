#pragma once

#include <vector>

namespace clog {

//
// Provides methods to insert and erase in a sorted way.
// 
// Allowed to become unsorted if other insertion methods are used.
// 
// Duplicate elements are not allowed
//
template <class T>
class sometimes_sorted_vector : public std::vector<T>
{
public:

	using const_iterator = typename std::vector<T>::const_iterator;
	using iterator = typename std::vector<T>::iterator;
	using size_type = typename std::vector<T>::size_type;

	auto contains(T item) const -> bool
	{
		assert (std::is_sorted(cbegin(), cend()));

		return std::binary_search(cbegin(), cend(), item);
	}

	auto insert(T item) -> std::pair<iterator, bool>
	{
		assert (std::is_sorted(cbegin(), cend()));

		auto pos { std::upper_bound(begin(), end(), item) };

		if (pos != cbegin() && *(pos-1) == item)
		{
			return { pos, false };
		}

		pos = std::vector<T>::insert(pos, item);

		return { pos, true };
	}

	auto erase(T item) -> size_type
	{
		assert (std::is_sorted(cbegin(), cend()));

		auto pos { std::lower_bound(cbegin(), cend(), item) };

		if (pos == cend()) return 0;

		std::vector<T>::erase(pos);

		return 1;
	}
};

} // clog
