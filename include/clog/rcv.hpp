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
** You can iterate over the items with visit(). The order of
** the items in the vector is not guaranteed, i.e.
**
**		a = v.acquire();
**		b = v.acquire();
**		v.visit([](auto item)
**		{
**			// b might be visited before a
**		});
**
** Adding or removing items from the vector doesn't invalidate
** indices. Everything logically stays where it is (though copy
** constructors may be called if the vector has to grow.)
**
** Therefore the index of an item can be used as a handle to
** retrieve it from the vector. The handle will never be
** invalidated until release(handle) is called.
**
** acquire() returns a handle to a new item.
** retrieve it using get().
**
**		rsv<thing> items;
**		rsv_handle item = items.acquire();
**		
**		// get() just returns a pointer to the object. do
**		// whatever you want with it
**		items.get(item)->bar();
**		*items.get(item) = thing{};
**		*items.get(item) = foo();
**		thing* ptr = items.get(item);
**		ptr->bar();
**
** release() removes the item at the given index (handle). The
** destructor is not called. The item is simply forgotten about.
** The destructor may be called later if the cell the item was
** occupying is reused (by a call to acquire().)
**
** destroy() destructs the object and then calls release(). it
** is equivalent to:
**
**		*get(index) = {};
**		release(index);
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

		current_.insert(index);

		return index;
	}

	auto release(handle_t index) -> void
	{
		current_.erase(index);
		
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
			if (!current_.contains(next_)) break;

			next_++;
		}

		return out;
	}

	size_t next_{};
	std::vector<T> cells_;

	// List of currently occupied indices
	// This is only used for iterating over the occupied cells
	vectors::sorted::unique::checked::vector<size_t> current_;
};

} // clog