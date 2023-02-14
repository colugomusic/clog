#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace clg {
namespace vectors {
namespace sorted {

// Check if the sorted vector contains the value.
// Precondition: The vector is sorted.
template <typename Begin, typename End, typename T, typename Compare = std::less<T>>
auto contains(Begin begin, End end, const T& value, Compare compare = Compare{}) -> bool
{
	assert (std::is_sorted(begin, end, compare));

	return std::binary_search(begin, end, value, compare);
}
template <typename T, typename Compare = std::less<T>>
auto contains(const std::vector<T>& vector, const T& value, Compare compare = Compare{}) -> bool
{
	return contains(std::cbegin(vector), std::cend(vector), value, compare);
}

// Erase all instances of the value from the sorted vector.
// Precondition: The vector is sorted.
template <typename T, typename Compare = std::less<T>>
auto erase_all(std::vector<T>* vector, const T& value, Compare compare = Compare{})
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	const auto [beg, end] = std::equal_range(std::cbegin(*vector), std::cend(*vector), value, compare);

	const auto count { std::distance(beg, end) };

	vector->erase(beg, end);

	return count;
}

// Return an iterator pointing to the first element equal to value, or end if not found.
// Precondition: The range is sorted.
template <typename Begin, typename End, typename T, typename Compare = std::less<T>>
auto find(Begin begin, End end, const T& value, Compare compare = Compare{})
{
	assert (std::is_sorted(begin, end, compare));

	const auto pos { std::lower_bound(begin, end, value, compare) };

	if (pos == end) return end;
	if (compare(value, *pos)) return end;

	return pos;
}

template <typename T, typename U, typename Compare = std::less<T>>
auto find(std::vector<T>& vector, const U& value, Compare compare = Compare{})
{
	return find(std::begin(vector), std::end(vector), value, compare);
}

template <typename T, typename U, typename Compare = std::less<T>>
auto find(const std::vector<T>& vector, const U& value, Compare compare = Compare{})
{
	return find(std::cbegin(vector), std::cend(vector), value, compare);
}

// Insert the value into the sorted vector.
// Precondition: The vector is sorted.
template <typename T, typename U, typename Compare = std::less<T>>
auto insert(std::vector<T>* vector, U&& value, Compare compare = Compare{}) -> std::pair<typename std::vector<T>::iterator, bool>
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	auto pos { std::upper_bound(std::begin(*vector), std::end(*vector), value, compare) };

	pos = vector->insert(pos, std::forward<U>(value));

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
template <typename T, typename U, typename Compare = std::less<T>>
auto insert(std::vector<T>* vector, U&& value, Compare compare = Compare{}) -> std::pair<typename std::vector<T>::iterator, bool>
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	auto pos { std::lower_bound(std::begin(*vector), std::end(*vector), value, compare) };

	if (pos != std::cend(*vector) && !compare(value, *pos))
	{
		return { pos, false };
	}

	pos = vector->insert(pos, std::forward<U>(value));

	return { pos, true };
}

// Insert the value into the sorted vector.
// If a value already exists that compares equal to it,
// it is overwritten.
// Precondition: The vector is sorted.
template <typename T, typename U, typename Compare = std::less<T>>
auto overwrite(std::vector<T>* vector, U&& value, Compare compare = Compare{})
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	auto pos { find(*vector, value, compare) };

	if (pos != std::cend(*vector))
	{
		*pos = std::forward<U>(value);
		return pos;
	}

	bool success;

	std::tie(pos, success) = insert(vector, std::forward<U>(value), compare);

	assert (success);

	return pos;
}

namespace checked {

// Insert the value into the sorted vector.
// Precondition: The vector is sorted.
// Precondition: The value does not already exist.
// Postcondition: The value was inserted.
template <typename T, typename U, typename Compare = std::less<T>>
auto insert(std::vector<T>* vector, U&& value, Compare compare = Compare{})
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	const auto [pos, success] = unique::insert(vector, std::forward<U>(value), compare);

	assert (success);

	return pos;
}

// Erase the value from the sorted vector.
// Precondition: The vector is sorted.
// Precondition: The value exists.
// Postcondition: Exactly one value was inserted.
template <typename T, typename Compare = std::less<T>>
auto erase(std::vector<T>* vector, const T& value, Compare compare = Compare{}) -> void
{
	assert (std::is_sorted(std::cbegin(*vector), std::cend(*vector), compare));

	const auto result = sorted::erase_all(vector, value, compare);

	assert (result == 1);
}

template <typename T, typename Compare = std::less<T>>
struct vector : public std::vector<T>
{
	using base_t = std::vector<T>;

	vector() = default;
	vector(const base_t& vec) : base_t{vec} {}
	vector(base_t && vec) : base_t{std::move(vec)} {}

	auto contains(const T& value) const -> bool
	{
		return clg::vectors::sorted::contains(static_cast<const base_t&>(*this), value, Compare{});
	}

	auto find(const T& value) const
	{
		return clg::vectors::sorted::find(static_cast<const base_t&>(*this), value, Compare{});
	}

	auto insert(const T& value) -> void
	{
		checked::insert(static_cast<base_t*>(this), value, Compare{});
	}

	auto insert(T&& value) -> void
	{
		checked::insert(static_cast<base_t*>(this), std::move(value), Compare{});
	}

	auto erase(const T& value) -> void
	{
		checked::erase(this, value, Compare{});
	}
};

} // checked
} // unique
} // sorted
} // vectors

namespace vs = vectors::sorted;
namespace vsu = vectors::sorted::unique;
namespace vsuc = vectors::sorted::unique::checked;

} // clg
