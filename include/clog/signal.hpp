#pragma once

#include <functional>
#include <unordered_set>
#include "stable_vector.hpp"

namespace clg {

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
	cn(detail::signal_base* signal, uint32_t handle);
	cn(cn && rhs) noexcept;
	~cn();
	auto operator=(cn&) -> cn& = delete;
	auto operator=(cn && rhs) noexcept -> cn&;
private:
	detail::cn_body body_{};
	uint32_t handle_{};
};

namespace detail {

struct signal_base
{
	virtual auto disconnect(uint32_t handle) -> void = 0;
	virtual auto update(uint32_t handle, detail::cn_body* body) -> void = 0;
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
		for (auto& cn : cns_) {
			cn.body->signal = this;
		}
	}
	auto operator=(signal&& rhs) noexcept -> signal& {
		cns_ = std::move(rhs.cns_);
		for (auto& cn : cns_) {
			cn.body->signal = this;
		}
		return *this;
	}
	~signal() {
		for (auto& cn : cns_) {
			cn.body->signal = {};
		}
	}
	template <typename Slot>
	[[nodiscard]] auto connect(Slot && slot) -> cn {
		cn_record record;
		record.cb = std::move(slot); 
		const auto handle{cns_.add(std::move(record))};
		return cn{this, handle};
	}
	template <typename Slot>
	[[nodiscard]] auto operator>>(Slot && slot) -> cn {
		return connect(std::forward<Slot>(slot));
	}
	auto operator()(Args... args) -> void {
		for (auto pos = cns_.begin(); pos != cns_.end(); pos++) {
			pos->cb(args...);
		}
	}
private:
	auto disconnect(uint32_t handle) -> void override {
		cns_.erase(handle);
	}
	auto update(uint32_t handle, detail::cn_body* body) -> void override {
		cns_[handle].body = body;
	}
	struct cn_record {
		detail::cn_body* body{};
		cb_t cb;
	};
	stable_vector<cn_record> cns_;
};

class store
{
public:
	auto operator+=(cn && c) -> void {
		connections_.push_back(std::move(c));
	}
	auto is_empty() const {
		return connections_.empty();
	}
private:
	std::vector<cn> connections_;
};

inline cn::cn(detail::signal_base* signal, uint32_t handle)
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

inline auto cn::operator=(cn && rhs) noexcept -> cn& {
	if (body_.signal) {
		body_.signal->disconnect(handle_);
	}
	body_ = rhs.body_;
	handle_ = rhs.handle_;
	rhs.body_ = {};
	if (!body_.signal) {
		return *this;
	}
	body_.signal->update(handle_, &body_);
	return *this;
}

inline cn::~cn() {
	if (!body_.signal) return;
	body_.signal->disconnect(handle_);
}

} // clg
