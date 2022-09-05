#pragma once

#include <cassert>
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

template <typename T, typename Compare = std::less<T>>
class lazy_sorted_vector
{
public:

	using vector_type = std::vector<T>;
	using const_iterator = typename vector_type::const_iterator;
	using const_reverse_iterator = typename vector_type::const_reverse_iterator;
	using iterator = typename vector_type::iterator;
	using reverse_iterator = typename vector_type::reverse_iterator;
	using size_type = typename vector_type::size_type;

	auto insert(T value) -> void
	{
		// Just push onto the end of the vector. It will
		// be sorted into the correct place later.
		vector_.push_back(value);

		lazy_sort();
	}

	auto erase(T value) -> void
	{
		do_sort();

		auto pos { std::lower_bound(std::begin(vector_), std::end(vector_), value, comparator_) };

		assert (pos != std::end(vector_));

		// Swap to the end of the vector. It will be discarded later.
		std::iter_swap(pos, std::end(vector_)-1);

		remove_count_++;

		lazy_sort();
	}

	auto begin() -> iterator { do_sort(); return std::begin(vector_); }
	auto end() -> iterator { do_sort(); return std::end(vector_); }
	auto begin() const -> const_iterator { do_sort(); return std::begin(vector_); }
	auto end() const -> const_iterator { do_sort(); return std::end(vector_); }
	auto cbegin() const -> const_iterator { do_sort(); return std::cbegin(vector_); }
	auto cend() const -> const_iterator { do_sort(); return std::cend(vector_); }
	auto rbegin() -> reverse_iterator { do_sort(); return std::rbegin(vector_); }
	auto rend() -> reverse_iterator { do_sort(); return std::rend(vector_); }
	auto rbegin() const -> const_reverse_iterator { do_sort(); return std::rbegin(vector_); }
	auto rend() const -> const_reverse_iterator { do_sort(); return std::rend(vector_); }
	auto crbegin() const -> const_reverse_iterator { do_sort(); return std::crbegin(vector_); }
	auto crend() const -> const_reverse_iterator { do_sort(); return std::crend(vector_); }
	auto contains(T* core) const -> bool { do_sort(); return sorted::contains(vector_, core, comparator_); }
	auto empty() const { return vector_.empty(); }
	auto size() const { do_sort(); return vector_.size(); }
	auto lazy_sort() { sorted_ = false; }

private:

	auto do_sort() const -> void
	{
		if (sorted_) return;

		vector_.resize(vector_.size() - remove_count_);
		std::sort(std::begin(vector_), std::end(vector_), comparator_);

		sorted_ = true;
		remove_count_ = 0;
	}

	Compare comparator_;
	mutable size_type remove_count_{ 0 };
	mutable vector_type vector_;
	mutable bool sorted_{ true };
};

namespace checked {

// Insert the value into the sorted vector.
// Asserts that the value did not already exist.
// Precondition: The vector is sorted.
template <typename T>
auto insert(std::vector<T>* vector, T value) -> typename std::vector<T>::iterator
{
	const auto [pos, success] = unique::insert(vector, value);

	assert (success);

	return pos;
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
		return clog::vectors::sorted::contains(*this, value);
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
