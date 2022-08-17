#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace clog {

using expiry_task = std::function<void()>;
class expirable;

namespace detail {

struct I_expiry_cell_body
{
	virtual auto expire() -> void = 0;
};

class expiry_cell_body : public I_expiry_cell_body
{
public:

	expiry_cell_body(expirable* object, size_t cell);
	expiry_cell_body(expiry_cell_body && rhs) noexcept;
	expiry_cell_body& operator=(expiry_cell_body && rhs) noexcept;
	~expiry_cell_body();

private:

	expirable* object_;
	size_t cell_;
};

class expiry_observer_body : public expiry_cell_body
{
public:

	expiry_observer_body() = default;
	expiry_observer_body(expirable* object, size_t cell, clog::expiry_task expiry_task);
	expiry_observer_body(expiry_observer_body && rhs) noexcept;
	expiry_observer_body& operator=(expiry_observer_body && rhs) noexcept;

private:
	
	auto expire() -> void override;

	clog::expiry_task expiry_task_;
};

template <class T>
class expiry_pointer_body : public expiry_cell_body
{
public:

	expiry_pointer_body() = default;
	expiry_pointer_body(expirable* object, size_t cell, T* raw_ptr);
	expiry_pointer_body(expiry_pointer_body && rhs) noexcept;
	expiry_pointer_body& operator=(expiry_pointer_body && rhs) noexcept;

	auto get() { return raw_ptr_; }
	auto get() const { return raw_ptr_; }

private:
	
	auto expire() -> void override;
	auto on_expired(clog::expiry_task expiry_task) -> void;

	T* raw_ptr_{};
	clog::expiry_task expiry_task_;
};

} // detail

class expiry_observer
{
public:

	expiry_observer() = default;
	expiry_observer(std::unique_ptr<detail::expiry_observer_body> body);

private:

	std::unique_ptr<detail::expiry_observer_body> body_;
};

template <class T>
class expiry_pointer
{
public:

	expiry_pointer() = default;
	expiry_pointer(std::unique_ptr<detail::expiry_pointer_body<T>> body);

	auto get() -> T*;
	auto get() const -> const T*;
	auto& operator*() const { return *get(); }
	auto& operator*() { return *get(); }
	auto operator->() const { return get(); }
	auto operator->() { return get(); }
	auto is_expired() const -> bool { return get(); }
	auto on_expired(clog::expiry_task expiry_task) -> void;

private:

	std::unique_ptr<detail::expiry_pointer_body<T>> body_;
};

class expirable
{
public:

	~expirable();

	auto expire() -> void;
	auto is_expired() const -> bool;

	template <class T>
	auto make_expiry_pointer() const -> expiry_pointer<T>;
	auto make_expiry_observer(clog::expiry_task expiry_task) -> expiry_observer;
	auto observe_expiry(clog::expiry_task expiry_task) -> void;

protected:

	clog::expiry_task on_expired;

private:

	auto release(size_t cell) -> void;

	bool expired_{ false };

	struct cell_array
	{
		size_t next{ 0 };
		std::vector<detail::I_expiry_cell_body*> cells;

		auto get_empty_cell() -> size_t;
		auto release(size_t cell) -> void;
	};

	cell_array cells_;
	std::vector<clog::expiry_task> expiry_tasks_;

	friend class detail::expiry_cell_body;
};

namespace detail {

///////////////////////////////////////
/// expiry_cell_body
///////////////////////////////////////
inline expiry_cell_body::expiry_cell_body(expirable* object, size_t cell)
	: object_{ object }
	, cell_{ cell }
{
}

inline expiry_cell_body::expiry_cell_body(expiry_cell_body && rhs) noexcept
	: object_{ rhs.object_ }
	, cell_{ rhs.cell_ }
{
	rhs.object_ = {};
	rhs.cell_ = {};
}

inline expiry_cell_body& expiry_cell_body::operator=(expiry_cell_body && rhs) noexcept
{
	object_ = rhs.object_;
	cell_ = rhs.cell_;
	rhs.object_ = {};
	rhs.cell_ = {};

	return *this;
}

inline expiry_cell_body::~expiry_cell_body()
{
	if (!object_) return;

	object_->release(cell_);
}

///////////////////////////////////////
/// expiry_observer_body
///////////////////////////////////////
inline expiry_observer_body::expiry_observer_body(expirable* object, size_t cell, clog::expiry_task expiry_task)
	: expiry_cell_body{ object, cell }
	, expiry_task_{ expiry_task }
{
}

inline expiry_observer_body::expiry_observer_body(expiry_observer_body && rhs) noexcept
	: expiry_cell_body{ std::move(rhs) }
	, expiry_task_{ std::move(rhs.expiry_task_) }
{
	rhs.expiry_task_ = {};
}

inline expiry_observer_body& expiry_observer_body::operator=(expiry_observer_body && rhs) noexcept
{
	expiry_cell_body::operator=(std::move(rhs));

	expiry_task_ = rhs.expiry_task_;
	rhs.expiry_task_ = {};

	return *this;
}

inline auto expiry_observer_body::expire() -> void
{
	expiry_task_();
}

///////////////////////////////////////
/// expiry_pointer_body
///////////////////////////////////////
template <class T>
expiry_pointer_body<T>::expiry_pointer_body(expirable* object, size_t cell, T* raw_ptr)
	: expiry_cell_body{ object, cell }
	, raw_ptr_{ raw_ptr }
{
}

template <class T>
expiry_pointer_body<T>::expiry_pointer_body(expiry_pointer_body && rhs) noexcept
	: expiry_cell_body{ std::move(rhs) }
	, raw_ptr_{ rhs.raw_ptr_ }
	, expiry_task_{ rhs.expiry_task_ }
{
	rhs.raw_ptr_ = {};
	rhs.expiry_task_ = {};
}

template <class T>
expiry_pointer_body<T>& expiry_pointer_body<T>::operator=(expiry_pointer_body && rhs) noexcept
{
	expiry_cell_body::operator=(std::move(rhs));

	raw_ptr_ = rhs.raw_ptr_;
	expiry_task_ = rhs.expiry_task_;

	rhs.raw_ptr_ = {};
	rhs.expiry_task_ = {};

	return *this;
}

template <class T>
auto expiry_pointer_body<T>::expire() -> void
{
	if (expiry_task_)
	{
		expiry_task_();
	}

	object_ = {};
	raw_ptr_ = {};
}

template <class T>
auto expiry_pointer_body<T>::on_expired(clog::expiry_task expiry_task) -> void
{
	expiry_task_ = expiry_task;
}

} // detail

///////////////////////////////////////
/// expiry_observer
///////////////////////////////////////
inline expiry_observer::expiry_observer(std::unique_ptr<detail::expiry_observer_body> body)
	: body_{ std::move(body) }
{
}

///////////////////////////////////////
/// expiry_pointer
///////////////////////////////////////
template <class T>
expiry_pointer<T>::expiry_pointer(std::unique_ptr<detail::expiry_pointer_body<T>> body)
	: body_{ std::move(body) }
{
}

template <class T>
auto expiry_pointer<T>::get() -> T*
{
	return body_.get()->get();
}

template <class T>
auto expiry_pointer<T>::get() const -> const T*
{
	return body_.get()->get();
}

template <class T>
auto expiry_pointer<T>::on_expired(clog::expiry_task expiry_task) -> void
{
	body_->get()->on_expired(expiry_task);
}

///////////////////////////////////////
/// expirable
///////////////////////////////////////
inline expirable::~expirable()
{
	if (!expired_)
	{
		expire();
	}
}

inline auto expirable::expire() -> void
{
	if (expired_) return;

	if (on_expired) on_expired();

	for (auto cell : cells_.cells)
	{
		if (!cell) continue;

		cell->expire();
	}

	for (auto expiry_task : expiry_tasks_)
	{
		expiry_task();
	}

	expired_ = true;
}

inline auto expirable::is_expired() const -> bool
{
	return expired_;
}

template <class T>
auto expirable::make_expiry_pointer() const -> expiry_pointer<T>
{
	const auto cell { pointers_.get_empty_cell() };

	auto body { std::make_unique<expiry_pointer_body<T>>(this, cell, static_cast<T*>(this)) };

	pointers_.cells[cell].body = body.get();

	return { std::move(body) };
}

inline auto expirable::make_expiry_observer(clog::expiry_task expiry_task) -> expiry_observer
{
	const auto cell { cells_.get_empty_cell() };

	auto body { std::make_unique<detail::expiry_observer_body>(this, cell, expiry_task) };

	cells_.cells[cell] = body.get();

	return { std::move(body) };
}

inline auto expirable::observe_expiry(clog::expiry_task expiry_task) -> void
{
	expiry_tasks_.push_back(expiry_task);
}

inline auto expirable::release(size_t cell) -> void
{
	cells_.release(cell);
}

///////////////////////////////////////
/// expirable::cell_array
///////////////////////////////////////
inline auto expirable::cell_array::get_empty_cell() -> size_t
{
	const auto out { next++ };

	while (true)
	{
		if (next >= cells.size()) break;
		if (!cells[next]) break;

		next++;
	}

	if (out >= cells.size())
	{
		cells.resize((out + 1) * 2);
	}

	return out;
}

inline auto expirable::cell_array::release(size_t cell) -> void
{
	cells[cell] = {};
	
	if (cell < next)
	{
		next = cell;
	}
}

} // clog