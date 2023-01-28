#pragma once

#include <functional>

namespace clg {

template <typename T>
class cached_base
{
public:

	cached_base() = default;
	cached_base(T initial_value) : value_{initial_value} {}

	auto set_dirty() { dirty_ = true; }

protected:

	mutable T value_{};
	mutable bool dirty_{true};
};

template <typename T>
class cached : public cached_base<T>
{
public:

	using fn_t = std::function<T()>;

	cached() = default;
	cached(T initial_value) : cached_base<T>{initial_value} {}
	cached(fn_t fn) : fn_{fn} {}
	cached(T initial_value, fn_t fn) : cached_base<T>{initial_value}, fn_{fn} {}

	auto operator=(fn_t fn) -> cached&
	{
		fn_ = fn;

		return *this;
	}

	auto operator=(T value) -> cached&
	{
		cached_base<T>::value_ = value;
		cached_base<T>::dirty_ = false;

		return *this;
	}

	auto get() const -> const T&
	{
		if (cached_base<T>::dirty_)
		{
			cached_base<T>::value_ = fn_();
			cached_base<T>::dirty_ = false;
		}

		return cached_base<T>::value_;
	}

	auto operator*() const { return get(); }
	auto operator->() const { return &get(); }
	operator T() const { return get(); }
	operator const T&() const { return get(); }

private:

	fn_t fn_;
};

template <typename T, typename... Args>
class cached<T(Args...)> : public cached_base<T>
{
public:

	using fn_t = std::function<T(Args...)>;

	cached() = default;
	cached(T initial_value) : cached_base<T>{initial_value} {}
	cached(fn_t fn) : fn_{fn} {}
	cached(T initial_value, fn_t fn) : cached_base<T>{initial_value}, fn_{fn} {}

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

	template <typename... Urgs>
	auto get(Urgs&&... args) const -> const T&
	{
		if (dirty_)
		{
			value_ = fn_(std::forward<Urgs>(args)...);
			dirty_ = false;
		}

		return value_;
	}

private:

	fn_t fn_;
};

} // clg