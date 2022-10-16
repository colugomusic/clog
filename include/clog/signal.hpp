#pragma once

#include <functional>
#include "rcv.hpp"

namespace clog {

class signal_base;

class cn
{
public:

	cn();
	cn(signal_base* s, rcv_handle handle);
	cn(cn && rhs) noexcept;
	~cn();

	auto operator=(cn && rhs) noexcept -> cn&;

private:

	signal_base* s_{};
	rcv_handle handle_;
};

class signal_base
{
public:

	virtual auto disconnect(rcv_handle handle) -> void = 0;
};

template <typename ... Args>
class signal : public signal_base
{
	using cb_t = std::function<void(Args...)>;

public:

	template <typename Slot>
	[[nodiscard]] auto connect(Slot && slot) -> cn
	{
		const auto handle { callbacks_.acquire() };

		*callbacks_.get(handle) = std::move(slot);

		return { this, handle };
	}

	template <typename Slot>
	[[nodiscard]] auto operator>>(Slot && slot) -> cn
	{
		return connect(std::forward<Slot>(slot));
	}

	auto disconnect(rcv_handle handle) -> void override
	{
		callbacks_.release(handle);
	}

	auto operator()(Args... args) -> void
	{
		callbacks_.visit([args...](cb_t cb)
		{
			cb(args...);
		});
	}

private:

	rcv<cb_t> callbacks_;
};

class store
{
public:

	auto operator+=(cn && c) -> void
	{
		connections_.push_back(std::move(c));
	}

private:

	std::vector<cn> connections_;
};

inline cn::cn() = default;

inline cn::cn(signal_base* s, rcv_handle handle)
	: s_{s}
	, handle_{handle}
{
}

inline cn::cn(cn && rhs) noexcept
	: s_{rhs.s_}
	, handle_{rhs.handle_}
{
	rhs.s_ = {};
}

inline auto cn::operator=(cn && rhs) noexcept -> cn&
{
	s_ = rhs.s_;
	handle_ = rhs.handle_;
	rhs.s_ = {};
	return *this;
}

inline cn::~cn()
{
	if (!s_) return;

	s_->disconnect(handle_);
}

} // clog