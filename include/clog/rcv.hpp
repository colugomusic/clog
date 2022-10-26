#pragma once

#include <new>
#include "vectors.hpp"

namespace clog {
namespace detail {

template<typename T>
class rcv_allocator : public std::allocator<uint8_t>
{
public:
	static std::align_val_t constexpr ALIGNMENT{ alignof(T) };

	[[nodiscard]] static auto allocate(std::size_t n) -> uint8_t*
	{
		if (n > std::numeric_limits<std::size_t>::max())
		{
			throw std::bad_array_new_length();
		}

		return reinterpret_cast<uint8_t*>(::operator new[](n, ALIGNMENT));
	}

	static auto deallocate(uint8_t* ptr, [[maybe_unused]] std::size_t n) -> void
	{
		::operator delete[](ptr, ALIGNMENT);
	}
};

template <typename T>
struct rcv_buffer
{
public:

	template <typename... ConstructorArgs>
	auto construct_at(size_t index, ConstructorArgs... constructor_args) -> T&
	{
		const auto memory { get_memory_for_cell(index) };
		const auto ptr { new(memory) T(constructor_args...) };

		return *ptr;
	}

	auto destruct_at(size_t index) -> void
	{
		get_ptr_to_item(index)->~T();
	}

	auto operator[](size_t index) -> T&
	{
		return *get_ptr_to_item(index);
	}

	auto operator[](size_t index) const -> const T&
	{
		return *get_ptr_to_item(index);
	}

	auto resize(size_t size) -> void
	{
		buffer_.resize(size * sizeof(T));
	}

	auto size() const -> size_t
	{
		return buffer_.size() / sizeof(T);
	}

private:

	auto get_memory_for_cell(size_t index) -> uint8_t*
	{
		assert (index < size());
		return buffer_.data() + (index * sizeof(T));
	}

	auto get_memory_for_cell(size_t index) const -> const uint8_t*
	{
		assert (index < size());
		return buffer_.data() + (index * sizeof(T));
	}

	auto get_ptr_to_item(size_t index) -> T*
	{
		return reinterpret_cast<T*>(get_memory_for_cell(index));
	}

	auto get_ptr_to_item(size_t index) const -> const T*
	{
		return reinterpret_cast<T*>(get_memory_for_cell(index));
	}

	std::vector<uint8_t, rcv_allocator<T>> buffer_;
};

} // detail

struct rcv_default_resize_strategy
{
	template <typename T>
	static auto resize(detail::rcv_buffer<T>* buffer, size_t required_size) -> void
	{
		buffer->resize(required_size * 2);
	}
};

using rcv_handle = size_t;

template <typename T, typename ResizeStrategy = rcv_default_resize_strategy>
class rcv
{
public:

	using handle_t = rcv_handle;

	rcv() = default;
	rcv(const rcv& rhs) = default;
	rcv(rcv&& rhs) = default;

	template <typename... ConstructorArgs>
	auto acquire(ConstructorArgs... constructor_args) -> handle_t
	{
		const auto index { next() };

		if (index >= buffer_.size())
		{
			ResizeStrategy::resize(&buffer_, index + 1);
		}

		current_.insert(index);
		buffer_.construct_at(index, constructor_args...);

		return index;
	}

	auto release(handle_t index) -> void
	{
		current_.erase(index);
		buffer_.destruct_at(index);
		
		if (index < next_)
		{
			next_ = index;
		}
	}

	auto get(handle_t index) -> T*
	{
		assert (clog::vectors::sorted::contains(current_, index));
		return &buffer_[index];
	}

	template <typename Visitor>
	auto visit(Visitor visitor) -> void
	{
		const auto current { current_ };

		for (auto index : current)
		{
			visitor(buffer_[index]);
		}
	}

private:

	auto next() -> size_t
	{
		namespace cvs = clog::vectors::sorted;

		const auto out { next_++ };
		auto check_beg { std::cbegin(current_) };
		auto check_end { std::cend(current_) };

		while (true)
		{
			if (next_ >= buffer_.size()) break;

			// Check if this cell is occupied
			check_beg = cvs::find(check_beg, check_end, next_);

			if (check_beg == check_end) break;

			next_++;
			check_beg++;
		}

		return out;
	}

	size_t next_{};
	detail::rcv_buffer<T> buffer_{};

	// List of currently occupied indices
	// This is only used for iterating over the occupied cells
	vectors::sorted::unique::checked::vector<size_t> current_;
};

} // clog