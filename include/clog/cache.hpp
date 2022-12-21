#pragma once

#include <functional>

namespace clg {

template <typename T>
class cached
{
public:

	using fn_t = std::function<T()>;

	cached() = default;
	cached(T initial_value) : value_{initial_value} {}
	cached(fn_t fn) : fn_{fn} {}
	cached(T initial_value, fn_t fn) : value_{initial_value}, fn_{fn} {}

	auto operator=(fn_t fn) -> cached&
	{
		fn_ = fn;

		return *this;
	}

	auto operator=(T value) -> cached&
	{
		value_ = value;
		dirty_ = false;

		return *this;
	}

	auto get() const -> const T&
	{
		if (dirty_)
		{
			value_ = fn_();
			dirty_ = false;
		}

		return value_;
	}

	auto set_dirty()
	{
		dirty_ = true;
	}

	auto operator*() const
	{
		return get();
	}

	auto operator->() const
	{
		return &get();
	}

	operator T() const
	{
		return get();
	}

	operator const T&() const
	{
		return get();
	}

private:

	mutable T value_{};
	mutable bool dirty_{true};
	fn_t fn_;
};

} // clg