#pragma once

#include <functional>
#include "rcv.hpp"

namespace clog {

namespace detail {

struct signal_base;

struct cn_body
{
	signal_base* signal{};
	rcv_handle handle;
};

} // detail

class cn
{
public:

	cn() = default;
	cn(detail::cn_body* body);
	cn(cn && rhs) noexcept;
	~cn();

	auto operator=(cn && rhs) noexcept -> cn&;

private:

	detail::cn_body* body_;
};

namespace detail {

struct signal_base
{
	virtual auto disconnect(cn_body* cn) -> void = 0;
};

} // detail

template <typename ... Args>
class signal : public detail::signal_base
{
	using cb_t = std::function<void(Args...)>;

public:

	~signal()
	{
		cns_.visit([](cn_record& record)
		{
			record.body.signal = {};
		});
	}

	template <typename Slot>
	[[nodiscard]] auto connect(Slot && slot) -> cn
	{
		const auto handle { cns_.acquire() };

		auto record { cns_.get(handle) };

		record->body.signal = this;
		record->body.handle = handle;
		record->cb = std::move(slot);

		return { &record->body };
	}

	template <typename Slot>
	[[nodiscard]] auto operator>>(Slot && slot) -> cn
	{
		return connect(std::forward<Slot>(slot));
	}

	auto disconnect(detail::cn_body* cn) -> void override
	{
		cns_.release(cn->handle);
	}

	auto operator()(Args... args) -> void
	{
		cns_.visit([args...](const cn_record& record)
		{
			record.cb(args...);
		});
	}

private:

	struct cn_record
	{
		detail::cn_body body;
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

inline cn::cn(detail::cn_body* body)
	: body_{body}
{
}

inline cn::cn(cn && rhs) noexcept
	: body_{rhs.body_}
{
	rhs.body_ = {};
}

inline auto cn::operator=(cn && rhs) noexcept -> cn&
{
	body_ = rhs.body_;
	rhs.body_ = {};
	return *this;
}

inline cn::~cn()
{
	if (!body_) return;
	if (!body_->signal) return;

	body_->signal->disconnect(body_);
}

} // clog