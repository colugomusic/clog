#pragma once

#include <functional>
#include "rcv.hpp"

namespace clog {

namespace detail {

struct signal_base;

struct cn_body
{
	signal_base* signal{};
};

} // detail

class cn
{
public:

	cn() = default;
	cn(const cn&) = delete;
	cn(detail::signal_base* signal, rcv_handle handle);
	cn(cn && rhs) noexcept;
	~cn();

	auto operator=(cn&) -> cn& = delete;
	auto operator=(cn && rhs) noexcept -> cn&;

private:

	detail::cn_body body_{};
	rcv_handle handle_{};
};

namespace detail {

struct signal_base
{
	virtual auto disconnect(rcv_handle handle) -> void = 0;
	virtual auto update(rcv_handle handle, detail::cn_body* body) -> void = 0;
};

} // detail

template <typename ... Args>
class signal : public detail::signal_base
{
	using cb_t = std::function<void(Args...)>;

public:

	signal() = default;
	signal(const signal&) = delete;
	signal(signal&& rhs) noexcept
		: cns_{ std::move(rhs.cns_) }
	{
		cns_.visit([this](cn_record& record)
		{
			record.body->signal = this;
		});
	}

	~signal()
	{
		cns_.visit([](cn_record& record)
		{
			record.body->signal = {};
		});
	}

	template <typename Slot>
	[[nodiscard]] auto connect(Slot && slot) -> cn
	{
		const auto handle { cns_.acquire() };

		auto record { cns_.get(handle) };

		record->cb = std::move(slot);

		return { this, handle };
	}

	template <typename Slot>
	[[nodiscard]] auto operator>>(Slot && slot) -> cn
	{
		return connect(std::forward<Slot>(slot));
	}

	auto disconnect(rcv_handle handle) -> void override
	{
		cns_.release(handle);
	}

	auto operator()(Args... args) -> void
	{
		cns_.visit([args...](const cn_record& record)
		{
			record.cb(args...);
		});
	}

private:

	auto update(rcv_handle handle, detail::cn_body* body) -> void override
	{
		cns_.get(handle)->body = body;
	}

	struct cn_record
	{
		detail::cn_body* body{};
		cb_t cb;
	};

	rcv<cn_record> cns_;
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

inline cn::cn(detail::signal_base* signal, rcv_handle handle)
	: handle_{handle}
{
	body_.signal = signal;

	signal->update(handle_, &body_);
}

inline cn::cn(cn && rhs) noexcept
	: body_{rhs.body_}
	, handle_{rhs.handle_}
{
	rhs.body_ = {};
	if (!body_.signal) return;
	body_.signal->update(handle_, &body_);
}

inline auto cn::operator=(cn && rhs) noexcept -> cn&
{
	if (body_.signal)
	{
		body_.signal->disconnect(handle_);
	}

	body_ = rhs.body_;
	handle_ = rhs.handle_;
	rhs.body_ = {};

	if (!body_.signal) return *this;

	body_.signal->update(handle_, &body_);

	return *this;
}

inline cn::~cn()
{
	if (!body_.signal) return;
	body_.signal->disconnect(handle_);
}

} // clog
