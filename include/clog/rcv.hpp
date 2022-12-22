#pragma once

#include <cstring>
#include <new>
#include "vectors.hpp"

namespace clg {
namespace detail {

template <typename AlignAs>
[[nodiscard]] static inline auto allocate(std::size_t n) -> std::byte*
{
	static constexpr std::align_val_t ALIGNMENT{ alignof(AlignAs) };
	return reinterpret_cast<std::byte*>(::operator new(n, ALIGNMENT));
}

template <typename AlignAs>
static inline auto deallocate(std::byte* ptr) -> void
{
	static constexpr std::align_val_t ALIGNMENT{ alignof(AlignAs) };
	::operator delete(ptr, ALIGNMENT);
}

template <typename T>
struct rcv_buffer
{
public:

	rcv_buffer() = default;

	rcv_buffer(size_t size)
		: buffer_size_{size * sizeof(T)}
		, buffer_{allocate<T>(buffer_size_)}
	{
	}

	rcv_buffer(rcv_buffer&& rhs) noexcept
		: buffer_{rhs.buffer_}
		, buffer_size_{rhs.buffer_size_}
	{
		rhs.buffer_size_ = 0;
	}

	~rcv_buffer()
	{
		if (buffer_size_ == 0) return;

		assert (buffer_size_ % sizeof(T) == 0);

		deallocate<T>(buffer_);
	}

	auto operator=(rcv_buffer&& rhs) -> rcv_buffer&
	{
		if (buffer_size_ > 0)
		{
			deallocate<T>(buffer_);
		}

		buffer_ = rhs.buffer_;
		buffer_size_ = rhs.buffer_size_;
		rhs.buffer_size_ = 0;
		return *this;
	}

	auto set(const rcv_buffer& rhs) -> void
	{
		assert (buffer_size_ >= rhs.buffer_size_);
		memcpy(buffer_, rhs.buffer_, rhs.buffer_size_);
	}

	template <typename... ConstructorArgs>
	auto construct_at(size_t index, ConstructorArgs&&... constructor_args) -> T&
	{
		assert (index < size());
		return construct_at(buffer_, index, std::forward<ConstructorArgs>(constructor_args)...);
	}

	auto destruct_at(size_t index) -> void
	{
		assert (index < size());
		get_ptr_to_item(index)->~T();
	}

	auto operator[](size_t index) -> T&
	{
		assert (index < size());
		return *get_ptr_to_item(index);
	}

	auto operator[](size_t index) const -> const T&
	{
		assert (index < size());
		return *get_ptr_to_item(index);
	}

	auto size() const -> size_t
	{
		return buffer_size_ / sizeof(T);
	}

private:

	template <typename... ConstructorArgs>
	static auto construct_at(std::byte* buffer, size_t index, ConstructorArgs&&... constructor_args) -> T&
	{
		const auto memory { get_memory_for_cell(buffer, index) };
		const auto ptr { new(memory) T(std::forward<ConstructorArgs>(constructor_args)...) };

		return *ptr;
	}

	static auto get_memory_for_cell(std::byte* buffer, size_t index) -> std::byte*
	{
		return buffer + (index * sizeof(T));
	}

	auto get_memory_for_cell(size_t index) -> std::byte*
	{
		assert (index < size());
		return get_memory_for_cell(buffer_, index);
	}

	auto get_memory_for_cell(size_t index) const -> const std::byte*
	{
		assert (index < size());
		return get_memory_for_cell(buffer_, index);
	}

	auto get_ptr_to_item(size_t index) -> T*
	{
		return reinterpret_cast<T*>(get_memory_for_cell(index));
	}

	auto get_ptr_to_item(size_t index) const -> const T*
	{
		return reinterpret_cast<const T*>(get_memory_for_cell(index));
	}

	size_t buffer_size_{0};
	std::byte* buffer_{};
};

// Find the next available empty cell. If there are
// no available cells, return buffer_.size().
//
// This algorithm isn't optimal (it's not necessary to
// call lower_bound over and over like that.)
//
// I have a headache though
template <typename Current>
auto find_next_empty_cell(const Current& current, size_t buffer_capacity, size_t* next) -> size_t
{
	namespace cvs = clg::vectors::sorted;

	const auto out { (*next)++ };
	auto check_beg { std::cbegin(current) };
	auto check_end { std::cend(current) };

	while (true)
	{
		if (*next >= buffer_capacity) break;

		// Check if this cell is occupied
		check_beg = std::lower_bound(check_beg, check_end, *next);

		if (check_beg == check_end) break;
		if (*check_beg != *next) break;

		(*next)++;
	}

	return out;
}

} // detail

struct rcv_default_resize_strategy
{
	static auto resize(size_t current_size, size_t required_size) -> size_t
	{
		if (current_size >= required_size) return current_size;

		return required_size * 2;
	}
};

using rcv_handle = size_t;

template <typename T, typename ResizeStrategy = rcv_default_resize_strategy>
class unsafe_rcv
{
public:

	using handle_t = rcv_handle;

	unsafe_rcv() = default;
	unsafe_rcv(unsafe_rcv&& rhs) = default;
	auto operator=(unsafe_rcv&& rhs) -> unsafe_rcv& = default;

	unsafe_rcv(const unsafe_rcv& rhs)
		: next_{rhs.next_}
		, buffer_{rhs.buffer_.size()}
		, current_{rhs.current_}
	{
		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			buffer_.set(rhs.buffer_);
			return;
		}

		for (const auto index : current_)
		{
			buffer_.construct_at(index, rhs.buffer_[index]);
		}
	}

	auto operator=(const unsafe_rcv& rhs) -> unsafe_rcv&
	{
		next_ = rhs.next_;
		buffer_ = rhs.buffer_.size();
		current_ = rhs.current_;

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			buffer_.set(rhs.buffer_);
			return *this;
		}

		for (const auto index : current_)
		{
			buffer_.construct_at(index, rhs.buffer_[index]);
		}

		return *this;
	}

	~unsafe_rcv()
	{
		for (const auto index : current_)
		{
			buffer_.destruct_at(index);
		}
	}

	auto active_handles() const -> std::vector<handle_t>
	{
		return current_;
	}

	auto capacity() const
	{
		return buffer_.size();
	}

	auto reserve(size_t size) -> void
	{
		resize(size);
	}

	auto size() const
	{
		return current_.size();
	}

	template <typename... ConstructorArgs>
	auto acquire(ConstructorArgs... constructor_args) -> handle_t
	{
		const auto index { next() };

		if (index >= buffer_.size())
		{
			resize(ResizeStrategy::resize(buffer_.size(), index + 1));
		}

		current_.insert(index);
		buffer_.construct_at(index, constructor_args...);

		return index;
	}

	template <typename... ConstructorArgs>
	auto acquire_at(size_t index, ConstructorArgs... constructor_args) -> handle_t
	{
		assert (!clg::vectors::sorted::contains(current_, index));

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
		assert (clg::vectors::sorted::contains(current_, index));
		return &buffer_[index];
	}

	auto get(handle_t index) const -> const T*
	{
		assert (clg::vectors::sorted::contains(current_, index));
		return &buffer_[index];
	}

private:

	auto next() -> size_t
	{
		return detail::find_next_empty_cell(current_, capacity(), &next_);
	}

	auto resize(size_t new_size) -> void
	{
		if (buffer_.size() >= new_size) return;

		detail::rcv_buffer<T> new_buffer{new_size};

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			new_buffer.set(buffer_);
			buffer_ = std::move(new_buffer);
			return;
		}

		for (const auto index : current_)
		{
			auto& item { buffer_[index] };

			if constexpr (std::is_nothrow_move_constructible_v<T>)
			{
				new_buffer.construct_at(index, std::move(item));
			}
			else
			{
				new_buffer.construct_at(index, item);
			}

			item.~T();
		}

		buffer_ = std::move(new_buffer);
	}

	size_t next_{};
	detail::rcv_buffer<T> buffer_{};

protected:
	
	// List of currently occupied indices
	vectors::sorted::unique::checked::vector<size_t> current_;
};

template <typename T, typename ResizeStrategy = rcv_default_resize_strategy>
class rcv : public unsafe_rcv<T, ResizeStrategy>
{
public:

	using handle_t = typename unsafe_rcv<T, ResizeStrategy>::handle_t;

	auto get(handle_t index) -> T*
	{
		if (!clg::vectors::sorted::contains(unsafe_rcv<T, ResizeStrategy>::current_, index))
		{
			return nullptr;
		}

		return unsafe_rcv<T, ResizeStrategy>::get(index);
	}
};

} // clg
