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

	expected(const Value& initial_value)
		: var_{initial_value}
	{
	}

	expected(Value&& initial_value)
		: var_{std::move(initial_value)}
	{
	}

	expected(const Error& error)
		: var_{error}
	{
	}

	expected(Error&& error)
		: var_{std::move(error)}
	{
	}

	auto operator=(const Value& value)
	{
		var_ = value;
		return *this;
	}

	auto operator=(Value&& value)
	{
		var_ = std::move(value);
		return *this;
	}

	auto operator=(const Error& error)
	{
		var_ = error;
		return *this;
	}

	auto operator=(Error&& error)
	{
		var_ = std::move(error);
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