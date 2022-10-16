#pragma once

#include "vectors.hpp"

namespace clog {

struct rcv_default_resize_strategy
{
	template <typename Container>
	static auto resize(Container* c, size_t required_size) -> void
	{
		c->resize(required_size * 2);
	}
};

using rcv_handle = size_t;

/*
** Reusable Cell Vector
** --------------------
**
** It's a vector of T which can only grow.
**
** T must be default constructible.
** T must be copy or move constructible.
**
** No memory is deallocated until destruction.
**
** Adding or removing items from the vector doesn't invalidate
** indices. Everything logically stays where it is (though copy
** constructors may be called if the vector has to grow.)
**
** Therefore the index of an item can be used as a handle to
** retrieve it from the vector. The handle will never be
** invalidated until release(handle) is called.
**
** acquire() returns a handle to the item. retrieve it using
** get(handle).
**
** release() removes the item at the given index (handle). The
** destructor is not called. The item is simply forgotten about.
** The destructor may be called later if the cell the item was
** occupying is reused (by a call to acquire().)
**
** destroy() destructs the object and then calls release(). it
** is equivalent to:
**		*get(index) = {};
**		release(index);
** 
*/
template <typename T, typename ResizeStrategy = rcv_default_resize_strategy>
class rcv
{
public:

	using handle_t = rcv_handle;

	auto acquire() -> handle_t
	{
		const auto index { next() };

		if (index >= cells_.size())
		{
			ResizeStrategy::resize(&cells_, index + 1);
		}

		vectors::sorted::unique::checked::insert(&current_, index);

		return index;
	}

	auto release(handle_t index) -> void
	{
		vectors::sorted::unique::checked::erase(&current_, index);
		
		if (index < next_)
		{
			next_ = index;
		}
	}

	auto destroy(handle_t index) -> void
	{
		cells_[index] = {};
		release();
	}

	auto get(handle_t index) -> T*
	{
		return &cells_[index];
	}

	template <typename Visitor>
	auto visit(Visitor visitor) -> void
	{
		const auto current { current_ };

		for (auto index : current)
		{
			visitor(cells_[index]);
		}
	}

private:

	auto next() -> size_t
	{
		const auto out { next_++ };

		while (true)
		{
			if (next_ >= cells_.size()) break;
			if (!vectors::sorted::contains(current_, next_)) break;

			next_++;
		}

		return out;
	}

	size_t next_{};
	std::vector<T> cells_;
	vectors::sorted::unique::checked::vector<size_t> current_;
};

} // clog