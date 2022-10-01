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

} // detail

class expiry_observer
{
public:

	expiry_observer() = default;
	expiry_observer(std::unique_ptr<detail::expiry_observer_body> body);

private:

	std::unique_ptr<detail::expiry_observer_body> body_;
};

class expirable
{
public:

	~expirable();

	auto expire() -> void;
	auto is_expired() const -> bool;

	auto make_expiry_observer(clog::expiry_task expiry_task) -> expiry_observer;
	auto observe_expiry(clog::expiry_task expiry_task) -> void;

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

} // detail

///////////////////////////////////////
/// expiry_observer
///////////////////////////////////////
inline expiry_observer::expiry_observer(std::unique_ptr<detail::expiry_observer_body> body)
	: body_{ std::move(body) }
{
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

	expired_ = true;

	for (auto cell : cells_.cells)
	{
		if (!cell) continue;

		// Release the cell and get the
		// expiry task if there is one
		const auto expiry_task { cell->expire() };

		if (!expiry_task) continue;

		expiry_task();
	}

	for (auto expiry_task : expiry_tasks_)
	{
		expiry_task();
	}
}

inline auto expirable::is_expired() const -> bool
{
	return expired_;
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
