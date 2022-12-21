#pragma once

#include <functional>
#include <vector>

namespace clg {

template <typename T>
class pool
{
public:

	using make_fn_t = std::function<T()>;

	struct acquire_result_t
	{
		T item;
		bool new_item;
	};

	pool()
		: make_fn_{[]() -> T { return T{}; }}
	{
	}

	pool(make_fn_t make_fn)
		: make_fn_{make_fn}
	{
	}

	auto acquire() -> acquire_result_t
	{
		if (pool_.empty())
		{
			return acquire_result_t{make_fn_(), true};
		}

		return acquire_result_t{get_item(), false};
	}

	auto set_make_fn(make_fn_t fn) -> void
	{
		make_fn_ = fn;
	}

	template <typename U>
	auto release(U&& item) -> void
	{
		pool_.push_back(std::move(item));
	}

	auto reserve(size_t capacity) -> void
	{
		pool_.reserve(capacity);
	}

private:

	auto get_item() -> T
	{
		assert (!pool_.empty());

		auto out{std::move(pool_.back())};

		pool_.pop_back();

		return out;
	}

	std::vector<T> pool_;
	make_fn_t make_fn_;
};

} // clg