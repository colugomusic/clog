#pragma once

#include <unordered_map>
#include "signal.hpp"

namespace clg {

class expiry_token
{
public:

	~expiry_token()
	{
		expire();
	}

	auto expire() -> void
	{
		if (expired_) return;

		expired_ = true;
		signal_();
	}

	auto is_expired() const -> bool
	{
		return expired_;
	}

	template <typename Slot>
	[[nodiscard]] auto observe_expiry(Slot && slot)
	{
		return signal_ >> std::forward<Slot>(slot);
	}

private:

	signal<> signal_;
	bool expired_{false};
};

class expirable
{
public:

	auto expire() -> void
	{
		token_.expire();
	}

	auto is_expired() const -> bool
	{
		return token_.is_expired();
	}

	template <typename Slot>
	[[nodiscard]] auto observe_expiry(Slot && slot)
	{
		return token_.observe_expiry(slot);
	}

	auto get_expiry_token() -> expiry_token& { return token_; }
	auto get_expiry_token() const -> const expiry_token& { return token_; }

private:

	expiry_token token_;
};

class expirable_with_custom_expiry_token
{
public:

	auto expire() -> void
	{
		get_expiry_token().expire();
	}

	auto is_expired() const -> bool
	{
		return get_expiry_token().is_expired();
	}

	template <typename Slot>
	[[nodiscard]] auto observe_expiry(Slot && slot)
	{
		return get_expiry_token().observe_expiry(slot);
	}

private:

	virtual auto get_expiry_token() -> expiry_token& = 0;
	virtual auto get_expiry_token() const -> const expiry_token& = 0;
};

template <typename Expirable, typename Slot>
auto observe_expiry(Expirable* object, Slot && slot) { return object->observe_expiry(std::forward<Slot>(slot)); }

template <typename T> struct attach { T object; auto operator->() const { return object; } operator T() const { return object; } };
template <typename T> struct detach { T object; auto operator->() const { return object; } operator T() const { return object; } };

template <typename T>
class attacher
{
public:

	template <typename U>
	auto operator<<(U object) -> void
	{
		attach(object);
	}

	template <typename U>
	auto operator>>(U object) -> void
	{
		detach(object);
	}

private:

	template <typename U>
	auto attach(U object) -> void
	{
		const auto on_expired = [=]()
		{
			detach(object);
		};

		static_cast<T*>(this)->update(clg::attach<U>{object});
		attached_objects_[std::hash<U>()(object)] = observe_expiry(object, on_expired);
	}

	template <typename U>
	auto detach(U object) -> void
	{
		attached_objects_.erase(std::hash<U>()(object));
		static_cast<T*>(this)->update(clg::detach<U>{object});
	}

	std::unordered_map<size_t, cn> attached_objects_;
};

} // clg