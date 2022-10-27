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

	rcv_buffer(rcv_buffer&& rhs) noexcept
		: buffer_{rhs.buffer_}
		, buffer_size_{rhs.buffer_size_}
	{
		rhs.buffer_size_ = 0;
	}

	rcv_buffer(const rcv_buffer& rhs)
		: buffer_size_{rhs.buffer_size_}
	{
		if (buffer_size_ == 0) return;

		buffer_ = allocate<T>(buffer_size_);

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			memcpy(buffer_, rhs.buffer_, buffer_size_);
		}
		else
		{
			for (size_t i = 0; i < size(); i++)
			{
				auto item { rhs.get_ptr_to_item(i) };

				construct_at(buffer_, i, *item);

				item->~T();
			}
		}
	}

	~rcv_buffer()
	{
		if (buffer_size_ == 0) return;

		assert (buffer_size_ % sizeof(T) == 0);

		deallocate<T>(buffer_);
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

	auto resize(size_t size) -> void
	{
		if (size <= this->size()) return;

		const auto new_buffer_size { size * sizeof(T) };
		const auto new_buffer { allocate<T>(new_buffer_size) };

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			memcpy(new_buffer, buffer_, buffer_size_);
		}
		else
		{
			for (size_t i = 0; i < this->size(); i++)
			{
				auto item { get_ptr_to_item(i) };

				if constexpr (std::is_nothrow_move_constructible_v<T>)
				{
					construct_at(new_buffer, i, std::move(*item));
				}
				else
				{
					construct_at(new_buffer, i, *item);
				}

				item->~T();
			}
		}

		deallocate<T>(buffer_);
		buffer_ = new_buffer;
		buffer_size_ = new_buffer_size;
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
		return reinterpret_cast<T*>(get_memory_for_cell(index));
	}

	uint8_t* buffer_{};
	size_t buffer_size_{0};
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

	~rcv()
	{
		deferred_release_ = current_;
		do_deferred_release();
	}

	auto capacity() const { return buffer_.size(); }
	auto size() const { return current_.size(); }

	auto reserve(size_t size) -> void
	{
		buffer_.resize(size);
	}

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
		if (visiting_ > 0)
		{
			deferred_release_.push_back(index);
			return;
		}

		do_release(index);
	}

	auto get(handle_t index) -> T*
	{
		assert (clog::vectors::sorted::contains(current_, index));
		return &buffer_[index];
	}

	template <typename Visitor>
	auto visit(Visitor visitor) -> void
	{
		visiting_++;

		to_visit_ = current_;

		for (auto index : to_visit_)
		{
			visitor(buffer_[index]);
		}

		if (--visiting_ > 0)
		{
			return;
		}

		do_deferred_release();
	}

private:

	auto do_deferred_release() -> void
	{
		while (!deferred_release_.empty())
		{
			to_delete_ = deferred_release_;
			deferred_release_.clear();

			for (auto index : to_delete_)
			{
				do_release(index);
			}

			// deferred_release_ might have more
			// stuff in it by now, so we loop
		}
	}

	auto do_release(handle_t index) -> void
	{
		current_.erase(index);
		buffer_.destruct_at(index);

		if (index < next_)
		{
			next_ = index;
		}
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

	size_t next_{};
	detail::rcv_buffer<T> buffer_{};

	// List of currently occupied indices
	// This is only used for iterating over the occupied cells
	vectors::sorted::unique::checked::vector<size_t> current_;
	std::vector<size_t> to_visit_;

	// release() might be called while visiting. If that happens
	// then don't release the cell right away, push it onto here
	// and the release will be deferred until we are done
	// visiting.
	// Additionally, release might be called again while
	// processing this list! So we keep processing it until it
	// is empty.
	std::vector<size_t> deferred_release_;
	std::vector<size_t> to_delete_;

	// >0 while visiting
	int visiting_{0};
};

} // clog