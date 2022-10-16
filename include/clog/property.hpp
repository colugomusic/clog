#pragma once

#include "signal.hpp"

namespace clog {

template <class T>
class get;

template <class T>
class setter
{
public:

	setter(setter && rhs) = default;

	setter(get<T>* property)
		: property_{ property }
	{
	}

	template <class U>
	auto& operator=(U && value)
	{
		set(std::forward<U>(value));

		return *this;
	}

	template <class U>
	auto set(U && value, bool notify = true, bool force = false) -> void
	{
		property_->set(std::forward<U>(value), notify, force);
	}

private:

	get<T>* property_;
};

template <class T>
class get
{
public:

	get() : value_ {} {}
	get(T value) : value_ { value } {}
	get(get<T> && rhs) = default;

	bool operator==(const T& value) const { return value_ == value; }
	auto notify() -> void { signal_(value_); }
	auto& get() const { return value_; }
	auto& operator*() const { return get(); }
	auto operator->() const { return &value_; }

	template <typename Slot>
	auto observe(Slot && slot) { return signal_.connect(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

private:

	template <class U>
	auto& operator=(U && value)
	{
		set(std::forward<U>(value));

		return *this;
	}

	template <class U>
	auto set(U && value, bool notify = true, bool force = false) -> void
	{
		if (value == value_ && !force) return;

		value_ = std::forward<U>(value);

		if (notify) this->notify();
	}

	friend class setter<T>;

	T value_;
	signal<T> signal_;
};

template <class T>
class setget : public get<T>
{
public:

	setget()
		: setter_{ this }
	{
	}

	setget(const T & value)
		: get<T> { value }
		, setter_{ this }
	{
	}

	setget(setget && rhs)
		: get<T> { std::move(rhs) }
		, setter_{ this }
	{
	}

	template <class U>
	auto& operator=(U && value)
	{
		setter_.operator=(std::forward<U>(value));

		return *this;
	}

	auto set(const T& value, bool notify = true, bool force = false) -> void
	{
		setter_.set(value, notify, force);
	}

private:

	setter<T> setter_;
};

template <class T>
class proxy_get
{
public:

	using fn_t = std::function<T()>;

	proxy_get() = default;
	proxy_get(fn_t fn) : fn_ { fn } {}

	auto operator=(fn_t fn) -> proxy_get&
	{
		fn_ = fn;
		return *this;
	}

	auto notify() -> void
	{
		signal_(fn_);
	}

	template <typename Slot>
	auto observe(Slot && slot) { return signal_.connect(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

	auto getter() const { return fn_; }
	auto get() const { return fn_(); }
	auto operator()() const { return get(); }
	auto operator*() const { return get(); }

private:

	fn_t fn_;
	signal<fn_t> signal_;
};

template <typename T> using property = setget<T>;
template <typename T> using proxy_property = proxy_get<T>;

} // clog