#pragma once

#include <variant>

namespace clog {

template <typename Value, typename Error>
struct expected
{
	expected() = default;
	expected(const expected& rhs) = default;
	expected(expected&& rhs) noexcept = default;
	auto operator=(const expected& rhs) -> expected& = default;
	auto operator=(expected&& rhs) noexcept -> expected& = default;

	template <typename T>
	expected(T&& initial_value)
		: var_{std::forward<T>(initial_value)}
	{
	}

	template <typename T>
	auto operator=(T&& value)
	{
		var_ = std::forward<T>(value);
		return *this;
	}

	operator bool() const
	{
		return std::holds_alternative<Value>(var_);
	}

	auto& get_error() const
	{
		return std::get<Error>(var_);
	}

	auto& get_value()
	{
		return std::get<Value>(var_);
	}

	auto& get_value() const
	{
		return std::get<Value>(var_);
	}

private:

	std::variant<std::monostate, Value, Error> var_;
};

} // clog