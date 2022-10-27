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

		do_deferred_disconnections();
		deferred_disconnect_ = cns_.active_handles();
		do_deferred_disconnections();
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
		if (visiting_ > 0)
		{
			deferred_disconnect_.push_back(handle);
			return;
		}

		cns_.release(handle);
	}

	auto operator()(Args... args) -> void
	{
		visiting_++;

		cns_.visit([args...](const cn_record& record)
		{
			record.cb(args...);
		});

		if (--visiting_ > 0) return;

		do_deferred_disconnections();
	}

private:

	auto do_deferred_disconnections() -> void
	{
		if (deferred_disconnect_.empty()) return;

		// We take a copy of the entire connection vector here, to handle
		// a corner case.

		// This signal object might ultimately be managed by some kind of
		// reference counting mechanism, e.g. perhaps it is a member of an
		// object being managed by a shared_ptr.

		// It is possible that the last remaining reference to the managed
		// object is owned by one of the callbacks!

		// Therefore when the slot is disconnected and the function object
		// is destroyed, we would be destroyed along with it, while we are
		// in the middle of doing things!

		// To prevent this, we make a copy of the connection vector so that
		// by the end of the disconnect loop, we still have at least one
		// reference to any reference counted objects owned by the
		// callbacks.
		const auto save_cns { cns_ };

		while (!deferred_disconnect_.empty())
		{
			to_disconnect_ = deferred_disconnect_;
			deferred_disconnect_.clear();

			for (const auto handle : to_disconnect_)
			{
				cns_.release(handle);
			}
		}
	}

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

	// >0 while visiting callbacks
	int visiting_{0};

	// disconnect() might be called while visiting,
	// so push the handle onto here to disconnect
	// it later
	std::vector<rcv_handle> deferred_disconnect_;
	std::vector<rcv_handle> to_disconnect_;
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
