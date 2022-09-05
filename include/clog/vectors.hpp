#pragma once

#include <vector>

namespace clog {
namespace vectors {
namespace sorted {

// Check if the sorted vector contains the value.
// Precondition: The vector is sorted.
template <typename T>
auto contains(const std::vector<T>& vector, T value) -> bool
{
	assert (std::is_sorted(std::cbegin(vector), std::cend(vector)));

	return std::binary_search(std::cbegin(vector), std::cend(vector), value);
}

template <typename T, typename Compare>
auto contains(const std::vector<T>& vector, T value, Compare compare) -> bool
{
	assert (std::is_sorted(std::cbegin(vector), std::cend(vector), compare));

	return std::binary_search(std::cbegin(vector), std::cend(vector), value, compare);
}

// Insert the value into the sorted vector.
// Precondition: The vector is sorted.
template <typename T>
auto insert(std::vector<T>* vector, T value) -> std::pair<typename std::vector<T>::iterator, bool>
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector)));

	auto pos { std::upper_bound(std::begin(*vector), std::end(*vector), value) };

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

// Erase all instances of the value from the sorted vector.
// Precondition: The vector is sorted.
template <typename T>
auto erase_all(std::vector<T>* vector, T value) -> typename std::vector<T>::size_type
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector)));

	auto beg { std::lower_bound(std::cbegin(*vector), std::cend(*vector), value) };

	if (beg == std::cend(*vector) || *beg != value) return 0;

	auto end { std::upper_bound(beg, std::cend(*vector), value) };

	const auto count { std::distance(beg, end) };

	vector->erase(beg, end);

	return count;
}

namespace unique {

// Insert the value into the sorted vector.
// Fails if the value already exists.
// Precondition: The vector is sorted.
template <typename T>
auto insert(std::vector<T>* vector, T value) -> std::pair<typename std::vector<T>::iterator, bool>
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector)));

	auto pos { std::upper_bound(std::begin(*vector), std::end(*vector), value) };

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
template <typename T>
auto insert(std::vector<T>* vector, T value) -> std::pair<typename std::vector<T>::iterator, bool>
{
	const auto [pos, success] = unique::insert(vector, value);

	assert (success);
}

// Erase the value from the sorted vector.
// Asserts that exactly one element was removed.
// Precondition: The vector is sorted.
template <typename T>
auto erase(std::vector<T>* vector, T value) -> void
{
	const auto result = sorted::erase_all(vector, value);

	assert (result == 1);
}

template <typename T>
struct vector : public std::vector<T>
{
	vector() = default;

	vector(std::vector<T> && vec)
		: std::vector<T>(std::forward<std::vector<T>>(vec))
	{
	}

	auto contains(T value) -> bool
	{
		clog::vectors::sorted::contains(this, value);
	}

	auto insert(T value) -> void
	{
		checked::insert(this, value);
	}

	auto erase(T value) -> void
	{
		checked::erase(this, value);
	}
};

} // checked
} // unique
} // sorted
} // vectors
} // clog
