#pragma once

#include <functional>
#include <unordered_map>
#include "auto_array.hpp"
#include "stable_vector.hpp"

namespace clg {

namespace detail {

struct cn_body {
	std::function<void(uint32_t)> disconnect;
	std::function<void(uint32_t, cn_body*)> update;
};

} // detail

class cn
{
public:
	cn() = default;
	cn(const cn&) = delete;
	cn(detail::cn_body body, uint32_t handle)
		: body_{std::move(body)}
		, handle_{handle}
	{
		body_.update(handle_, &body_);
	}
	cn(cn&& rhs) noexcept
		: body_{std::move(rhs.body_)}
		, handle_{rhs.handle_}
	{
		rhs.body_ = {};
		if (!body_.update) {
			return;
		}
		body_.update(handle_, &body_);
	}
	~cn() {
		if (!body_.disconnect) {
			return;
		}
		body_.disconnect(handle_);
	}
	auto operator=(cn&) -> cn& = delete;
	auto operator=(cn&& rhs) noexcept -> cn& {
		if (body_.disconnect) {
			body_.disconnect(handle_);
		}
		body_ = std::move(rhs.body_);
		handle_ = rhs.handle_;
		rhs.body_ = {};
		if (!body_.update) {
			return *this;
		}
		body_.update(handle_, &body_);
		return *this;
	}
private:
	detail::cn_body body_{};
	uint32_t handle_{};
};

template <typename ... Args>
class signal
{
	using cb_t = std::function<void(Args...)>;
public:
	signal() = default;
	signal(const signal&) = delete;
	signal(signal&& rhs) noexcept
		: cns_{ std::move(rhs.cns_) }
	{
		for (auto& cn : cns_) {
			update(cn.body);
		}
	}
	auto operator=(signal&& rhs) noexcept -> signal& {
		cns_ = std::move(rhs.cns_);
		for (auto& cn : cns_) {
			update(cn.body);
		}
		return *this;
	}
	~signal() {
		for (auto& cn : cns_) {
			(*cn.body) = {};
		}
	}
	template <typename Slot>
	[[nodiscard]] auto connect(Slot && slot) -> cn {
		cn_record record;
		record.cb = std::move(slot); 
		const auto handle{cns_.add(std::move(record))};
		detail::cn_body body;
		update(&body);
		return cn{std::move(body), handle};
	}
	template <typename Slot>
	[[nodiscard]] auto operator>>(Slot && slot) -> cn {
		return connect(std::forward<Slot>(slot));
	}
	auto operator()(Args... args) -> void {
		for (auto& cn : cns_) {
			cn.cb(args...);
		}
	}
private:
	auto update(detail::cn_body* body) -> void {
		body->disconnect = [this](uint32_t handle) {
			cns_.erase(handle);
		};
		body->update = [this](uint32_t handle, detail::cn_body* body) {
			cns_[handle].body = body;
		};
	}
	struct cn_record {
		detail::cn_body* body{};
		cb_t cb;
	};
	clg::stable_vector<cn_record> cns_;
};

class store {
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

struct watcher {
	template <typename Category>
	auto clear(Category category) -> void {
		clear(static_cast<size_t>(category));
	}
	auto clear(size_t category) -> void {
		stores_[category] = {};
	}
	template <typename Category>
	auto watch(Category category, clg::cn&& cn) -> void {
		watch(static_cast<size_t>(category), std::move(cn));
	}
	auto watch(size_t category, clg::cn&& cn) -> void {
		stores_[category] += std::move(cn);
	}
private:
	auto_array<clg::store> stores_;
};

template <typename Key>
struct key_watcher {
	template <typename Category>
	auto clear(Category category) -> void {
		clear(static_cast<size_t>(category));
	}
	template <typename Category>
	auto clear(Category category, Key key) -> void {
		clear(static_cast<size_t>(category), key);
	}
	auto clear(size_t category) -> void {
		stores_[category] = {};
	}
	auto clear(size_t category, Key key) -> void {
		key_stores_[category][key] = {};
	}
	template <typename Category>
	auto watch(Category category, clg::cn&& cn) -> void {
		watch(static_cast<size_t>(category), std::move(cn));
	}
	template <typename Category>
	auto watch(Category category, Key key, clg::cn&& cn) -> void {
		watch(static_cast<size_t>(category), key, std::move(cn));
	}
	auto watch(size_t category, clg::cn&& cn) -> void {
		stores_[category] += std::move(cn);
	}
	auto watch(size_t category, Key key, clg::cn&& cn) -> void {
		key_stores_[category][key] += std::move(cn);
	}
private:
	auto_array<clg::store> stores_;
	auto_array<std::unordered_map<Key, clg::store>> key_stores_;
};

} // clg