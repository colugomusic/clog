#pragma once

#include <cstdint>
#include <optional>

namespace clg {

template <typename T>
struct box
{
	using version_t = uint64_t;

	auto get_version() const { return version_; }

	template <typename FnT>
	auto update(FnT&& fn) -> void
	{
		value_ = fn(value_);
		version_++;
	}

	template <typename U>
	auto operator=(U&& value) -> void
	{
		value_ = value;
		version_++;
	}

	auto operator->() const -> const T* { return &value_; }
	auto operator*() const -> const T& { return value_; }

	friend auto operator==(const box<T>& a, const box<T>& b) -> bool {
		return a.version_ == b.version_;
	}

	friend auto operator!=(const box<T>& a, const box<T>& b) -> bool {
		return a.version_ != b.version_;
	}

private:
	version_t version_{0};
	T value_;
};

template <typename T>
struct optional_box
{
	using version_t = int64_t;

	optional_box() = default;

	template <typename U>
	optional_box(U&& initial_value) : value_{initial_value} {}

	auto get_version() const { return version_; }

	template <typename FnT>
	auto update(FnT&& fn) -> void
	{
		if (!value_)
		{
			value_ = fn(T{});
		}
		else
		{
			value_ = fn(value_.value());
		}

		version_++;
	}

	template <typename U>
	auto operator=(U&& value) -> void
	{
		value_ = value;
		version_++;
	}

	operator bool() const { return has_value(); }
	auto has_value() const { return value_.has_value(); }
	auto operator->() const -> const T* { return &value_.value(); }
	auto operator*() const -> const T& { return value_.value(); }

	friend auto operator==(const box<T>& a, const box<T>& b) -> bool {
		return a.version_ == b.version_;
	}

	friend auto operator!=(const box<T>& a, const box<T>& b) -> bool {
		return a.version_ != b.version_;
	}

private:
	version_t version_{0};
	std::optional<T> value_;
};

} // clg