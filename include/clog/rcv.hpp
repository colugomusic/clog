#pragma once

#include <new>
#include "vectors.hpp"

namespace clog {
namespace detail {

template <typename AlignAs>
[[nodiscard]] static inline auto allocate(std::size_t n) -> uint8_t*
{
	static constexpr std::align_val_t ALIGNMENT{ alignof(AlignAs) };
	return reinterpret_cast<uint8_t*>(::operator new(n, ALIGNMENT));
}

template <typename AlignAs>
static inline auto deallocate(uint8_t* ptr) -> void
{
	static constexpr std::align_val_t ALIGNMENT{ alignof(AlignAs) };
	::operator delete(ptr, ALIGNMENT);
}

template <typename T>
struct rcv_buffer
{
public:

	rcv_buffer() = default;

	rcv_buffer(size_t initial_size)
		: buffer_size_{initial_size * sizeof(T)}
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
	static auto construct_at(uint8_t* buffer, size_t index, ConstructorArgs&&... constructor_args) -> T&
	{
		const auto memory { get_memory_for_cell(buffer, index) };
		const auto ptr { new(memory) T(std::forward<ConstructorArgs>(constructor_args)...) };

		return *ptr;
	}

	static auto get_memory_for_cell(uint8_t* buffer, size_t index) -> uint8_t*
	{
		return buffer + (index * sizeof(T));
	}

	auto get_memory_for_cell(size_t index) -> uint8_t*
	{
		assert (index < size());
		return get_memory_for_cell(buffer_, index);
	}

	auto get_memory_for_cell(size_t index) const -> const uint8_t*
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
	uint8_t* buffer_{};
};

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
class rcv
{
public:

	using handle_t = rcv_handle;

	rcv() = default;
	rcv(rcv&& rhs) = default;

	rcv(const rcv& rhs)
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

	~rcv()
	{
		assert (!visiting_);

		for (const auto index : current_)
		{
			buffer_.destruct_at(index);
		}
	}

	auto active_cells() const -> std::vector<size_t>
	{
		return current_;
	}

	auto capacity() const
	{
		return buffer_.size();
	}

	auto reserve(size_t size) -> void
	{
		buffer_.resize(size);
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
		visit_begin();

		to_visit_ = current_;

		for (const auto index : to_visit_)
		{
			visitor(buffer_[index]);
		}

		visit_finish();
	}

private:

	auto visit_begin()
	{
#		if _DEBUG
		visiting_ = true;
#		endif
	}

	auto visit_finish()
	{
#		if _DEBUG
		visiting_ = false;
#		endif
	}

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
			check_beg = std::lower_bound(check_beg, check_end, next_);

			if (check_beg == check_end) break;
			if (*check_beg != next_) break;

			next_++;
		}

		return out;
	}

	auto resize(size_t new_size) -> void
	{
		if (buffer_.size() >= new_size) return;

		detail::rcv_buffer<T> new_buffer{new_size};

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			new_buffer_.set(buffer_);
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

	// List of currently occupied indices
	// This is only used for iterating over the occupied cells
	vectors::sorted::unique::checked::vector<size_t> current_;

	// Handles currently being visited
	std::vector<size_t> to_visit_;

#	if _DEBUG
	// true while visiting, for assertions
	bool visiting_{false};
#	endif
};

} // clog