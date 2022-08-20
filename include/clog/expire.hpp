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
	virtual auto expire() -> expiry_task = 0;
};

class expiry_cell_body : public I_expiry_cell_body
{
public:

	expiry_cell_body(expirable* object, size_t cell);
	expiry_cell_body(expiry_cell_body && rhs) noexcept;
	expiry_cell_body& operator=(expiry_cell_body && rhs) noexcept;
	~expiry_cell_body();

protected:

	auto release() -> void;

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
	
	auto expire() -> expiry_task override;

	clog::expiry_task expiry_task_{};
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

	friend inline auto operator == (const expiry_pointer_body<T>& lhs, const expiry_pointer_body<T>& rhs)
	{
		return lhs.raw_ptr_ == rhs.raw_ptr_;
	}

	friend inline auto operator == (const expiry_pointer_body<T>& lhs, const T* rhs)
	{
		return lhs.raw_ptr_ == rhs;
	}

private:
	
	auto expire() -> expiry_task override;
	auto on_expired(clog::expiry_task expiry_task) -> void;

	T* raw_ptr_{};
	clog::expiry_task expiry_task_{};

	friend struct std::hash<expiry_pointer_body<T>>;
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
	expiry_pointer(T* raw_ptr);
	expiry_pointer(const expiry_pointer<T>& rhs);
	expiry_pointer(std::unique_ptr<detail::expiry_pointer_body<T>> body);

	operator T*();

	auto clone() const -> expiry_pointer<T>;
	auto get() const -> T*;
	auto& operator*() const { return *get(); }
	//auto& operator*() { return *get(); }
	auto operator->() const { return get(); }
	//auto operator->() { return get(); }
	auto is_expired() const -> bool { return get(); }
	auto on_expired(clog::expiry_task expiry_task) -> void;

	friend inline auto operator == (const expiry_pointer<T>& lhs, const expiry_pointer<T>& rhs)
	{
		return *lhs.body_ == *rhs.body_;
	}

	friend inline auto operator == (const expiry_pointer<T>& lhs, const T* rhs)
	{
		return *lhs.body_ == rhs;
	}

	friend inline auto operator != (const expiry_pointer<T>& lhs, const T* rhs)
	{
		return !(lhs == rhs);
	}

	friend inline auto operator < (const expiry_pointer<T>& lhs, const expiry_pointer<T>& rhs)
	{
		return *lhs.body_ < *rhs.body_;
	}

private:

	std::unique_ptr<detail::expiry_pointer_body<T>> body_;

	friend struct std::hash<clog::expiry_pointer<T>>;
};

class expirable
{
public:

	~expirable();

	auto expire() -> void;
	auto is_expired() const -> bool;

	template <class T>
	auto make_expiry_pointer() -> expiry_pointer<T>;
	auto make_expiry_observer(clog::expiry_task expiry_task) -> expiry_observer;
	auto observe_expiry(clog::expiry_task expiry_task) -> void;

	template <class T>
	auto make_expiry_pointer_body() -> std::unique_ptr<detail::expiry_pointer_body<T>>;

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
	release();
}

inline auto expiry_cell_body::release() -> void
{
	if (!object_) return;

	object_->release(cell_);
	object_ = {};
	cell_ = {};
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

inline auto expiry_observer_body::expire() -> expiry_task
{
	expiry_cell_body::release();

	return expiry_task_;
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
auto expiry_pointer_body<T>::expire() -> expiry_task
{
	expiry_cell_body::release();

	raw_ptr_ = {};

	return expiry_task_;
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
expiry_pointer<T>::expiry_pointer(T* raw_ptr)
	: body_{ raw_ptr->make_expiry_pointer_body<T>() }
{
}

template <class T>
expiry_pointer<T>::expiry_pointer(const expiry_pointer<T>& rhs)
	: body_{ rhs.get()->make_expiry_pointer_body<T>() }
{
}

template <class T>
expiry_pointer<T>::operator T*()
{
	return get();
}

template <class T>
auto expiry_pointer<T>::clone() const -> expiry_pointer<T>
{
	return body_.get()->get()->make_expiry_pointer<T>();
}

//template <class T>
//auto expiry_pointer<T>::get() -> T*
//{
//	return body_.get()->get();
//}

template <class T>
auto expiry_pointer<T>::get() const -> T*
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

	for (auto cell : cells_.cells)
	{
		if (!cell) continue;

		const auto expiry_task { cell->expire() };

		expiry_task();
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
auto expirable::make_expiry_pointer() -> expiry_pointer<T>
{
	return { make_expiry_pointer_body() };
}

template <class T>
auto expirable::make_expiry_pointer_body() -> std::unique_ptr<detail::expiry_pointer_body<T>>
{
	const auto cell { cells_.get_empty_cell() };

	auto out { std::make_unique<detail::expiry_pointer_body<T>>(this, cell, static_cast<T*>(this)) };

	cells_.cells[cell] = out.get();

	return out;
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

namespace std {

template <typename T>
struct hash<clog::detail::expiry_pointer_body<T>>
{
    auto operator()(const clog::detail::expiry_pointer_body<T>& ptr) const -> std::size_t
    {
        return hash<T*>()(ptr.raw_ptr_);
    }
};

template <typename T>
struct hash<clog::expiry_pointer<T>>
{
    auto operator()(const clog::expiry_pointer<T>& ptr) const -> std::size_t
    {
        return hash<clog::detail::expiry_pointer_body<T>>()(*ptr.body_);
    }
};

} // std