#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace clog {
namespace vectors {
namespace sorted {

// Check if the sorted vector contains the value.
// Precondition: The vector is sorted.
template <typename T, typename Compare>
auto contains(const std::vector<T>& vector, T value, Compare compare = std::less<T>{}) -> bool
{
	assert (std::is_sorted(std::cbegin(vector), std::cend(vector), compare));

	return std::binary_search(std::cbegin(vector), std::cend(vector), value, compare);
}

// Erase all instances of the value from the sorted vector.
// Precondition: The vector is sorted.
template <typename T, typename Compare>
auto erase_all(std::vector<T>* vector, T value, Compare compare = std::less<T>{}) -> typename std::vector<T>::size_type
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	const auto [beg, end] = std::equal_range(std::cbegin(*vector), std::cend(*vector), value, compare);

	const auto count { std::distance(beg, end) };

	vector->erase(beg, end);

	return count;
}

// Return an iterator pointing to the first element equal to value, or end if not found.
// Precondition: The range is sorted.
template <typename Begin, typename End, typename T, typename Compare>
auto find(Begin begin, End end, T value, Compare compare = std::less<T>{}) -> typename std::vector<T>::const_iterator
{
	assert (std::is_sorted(begin, end, compare));

	const auto pos { std::lower_bound(begin, end, value, compare) };

	if (*pos != value) return end;

	return pos;
}

// Insert the value into the sorted vector.
// Precondition: The vector is sorted.
template <typename T, typename Compare>
auto insert(std::vector<T>* vector, T value, Compare compare = std::less<T>{}) -> std::pair<typename std::vector<T>::iterator, bool>
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	auto pos { std::upper_bound(std::begin(*vector), std::end(*vector), value, compare) };

	pos = vector->insert(pos, value);

	return { pos, true };
}

// Add the range to the vector and then sort the entire vector.
// The vector does not need to be pre-sorted.
template <typename T, typename Begin, typename End>
auto insert(std::vector<T>* vector, Begin begin, End end) -> void
{
	std::copy(begin, end, std::back_inserter(*vector));
	std::sort(std::begin(*vector), std::end(*vector));
}

namespace unique {

// Insert the value into the sorted vector.
// Fails if the value already exists.
// Precondition: The vector is sorted.
template <typename T, typename Compare>
auto insert(std::vector<T>* vector, T value, Compare compare = std::less<T>{}) -> std::pair<typename std::vector<T>::iterator, bool>
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	auto pos { std::upper_bound(std::begin(*vector), std::end(*vector), value, compare) };

	if (pos != std::cbegin(*vector) && *(pos-1) == value)
	{
		return { pos, false };
	}

	pos = vector->insert(pos, value);

	return { pos, true };
}

namespace checked {

// Insert the value into the sorted vector.
// Asserts that the value did not already exist.
// Precondition: The vector is sorted.
template <typename T, typename Compare>
auto insert(std::vector<T>* vector, T value, Compare compare = std::less<T>{}) -> typename std::vector<T>::iterator
{
	const auto [pos, success] = unique::insert(vector, value, compare);

	assert (success);

	return pos;
}

// Erase the value from the sorted vector.
// Asserts that exactly one element was removed.
// Precondition: The vector is sorted.
template <typename T, typename Compare>
auto erase(std::vector<T>* vector, T value, Compare compare = std::less<T>{}) -> void
{
	const auto result = sorted::erase_all(vector, value, compare);

	assert (result == 1);
}

template <typename T, typename Compare = std::less<T>>
struct vector : public std::vector<T>
{
	vector() = default;

	vector(std::vector<T> && vec)
		: std::vector<T>(std::forward<std::vector<T>>(vec))
	{
	}

	auto contains(T value) -> bool
	{
		return clog::vectors::sorted::contains(*this, value, Compare{});
	}

	auto insert(T value) -> void
	{
		checked::insert(this, value, Compare{});
	}

	auto erase(T value) -> void
	{
		checked::erase(this, value, Compare{});
	}
};

} // checked
} // unique
} // sorted
} // vectors
} // clog
