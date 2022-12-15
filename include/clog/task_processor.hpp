#pragma once

#include <functional>
#include <mutex>
#include "rcv.hpp"

namespace clog {

using task_t = std::function<void()>;
template <typename LockFreeQueue> class lock_free_task_pusher_static;
template <typename LockFreeQueue> class lock_free_task_pusher_dynamic;
class locking_task_pusher;

struct lock_free_task
{
	int dynamic_pusher_id;
	task_t task;
};

template <typename LockFreeQueue>
class lock_free_task_processor
{
public:

	auto make_pusher(size_t max_size) -> lock_free_task_pusher_static<LockFreeQueue>;
	auto process_all() -> void;

private:

	auto push(clog::rcv_handle handle, lock_free_task task) -> void;
	auto release_static_pusher(clog::rcv_handle handle) -> void;
	auto release_dynamic_pusher(clog::rcv_handle, int dynamic_pusher_id) -> void;
	auto next_dynamic_pusher_id() -> int;

	struct queue
	{
		queue(size_t max_size);
		queue(queue&&) noexcept = default;

		auto process_all() -> void;
		auto push(lock_free_task task) -> void;
		auto release_dynamic_pusher(int dynamic_pusher_id) -> void;

	private:

		LockFreeQueue queue_;
		std::vector<int> dead_dynamic_pushers_;
	};

	clog::unsafe_rcv<queue> queues_;
	int next_dynamic_pusher_id_{0};

	friend class lock_free_task_pusher_static<LockFreeQueue>;
};

template <typename LockFreeQueue>
class lock_free_task_pusher_static
{
public:

	lock_free_task_pusher_static() = default;
	lock_free_task_pusher_static(lock_free_task_pusher_static<LockFreeQueue>&& rhs) noexcept;
	lock_free_task_pusher_static(lock_free_task_processor<LockFreeQueue>* processor, clog::rcv_handle handle);
	auto operator=(lock_free_task_pusher_static<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher_static<LockFreeQueue>&;
	~lock_free_task_pusher_static();

	auto push(task_t task) -> void;
	auto make_pusher() -> lock_free_task_pusher_dynamic<LockFreeQueue>;
	auto release() -> void;

private:

	auto push(lock_free_task task) -> void;
	auto release_dynamic_pusher(int dynamic_pusher_id) -> void;

	lock_free_task_processor<LockFreeQueue>* processor_{};
	clog::rcv_handle handle_;

	friend class lock_free_task_pusher_dynamic<LockFreeQueue>;
};

template <typename LockFreeQueue>
class lock_free_task_pusher_dynamic
{
public:

	lock_free_task_pusher_dynamic() = default;
	lock_free_task_pusher_dynamic(lock_free_task_pusher_dynamic<LockFreeQueue>&& rhs) noexcept;
	lock_free_task_pusher_dynamic(lock_free_task_pusher_static<LockFreeQueue>* static_pusher, int dynamic_pusher_id);
	auto operator=(lock_free_task_pusher_dynamic<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher_dynamic<LockFreeQueue>&;
	~lock_free_task_pusher_dynamic();

	auto push(task_t task) -> void;
	auto release() -> void;

private:

	lock_free_task_pusher_static<LockFreeQueue>* static_pusher_{};
	int dynamic_pusher_id;
};

class locking_task_processor
{
public:

	auto make_pusher() -> locking_task_pusher;
	auto process_all() -> void;

private:

	auto push(clog::rcv_handle handle, task_t task) -> void;
	auto release(clog::rcv_handle handle) -> void;

	struct queue
	{
		queue() = default;
		queue(queue&& rhs) noexcept;

		auto process_all() -> void;
		auto push(task_t task) -> void;

	private:

		std::vector<task_t> queue_;
		std::mutex mutex_;
	};

	clog::unsafe_rcv<queue> queues_;
	std::mutex mutex_;

	friend class locking_task_pusher;
};

class locking_task_pusher
{
public:

	locking_task_pusher() = default;
	locking_task_pusher(locking_task_pusher&& rhs) noexcept;
	locking_task_pusher(locking_task_processor* processor, clog::rcv_handle handle);
	auto operator=(locking_task_pusher&& rhs) noexcept -> locking_task_pusher&;
	~locking_task_pusher();

	auto push(task_t task) -> void;
	auto release() -> void;

private:

	locking_task_processor* processor_{};
	clog::rcv_handle handle_;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor queue
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline lock_free_task_processor<LockFreeQueue>::queue::queue(size_t max_size)
	: queue_{max_size}
{
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::process_all() -> void
{
	lock_free_task task;

	while (queue_.pop(&task))
	{
		if (task.dynamic_pusher_id >= 0 && vs::contains(dead_dynamic_pushers_, task.dynamic_pusher_id)) continue;

		task.task();
	}

	dead_dynamic_pushers_.clear();
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::push(lock_free_task task) -> void
{
	queue_.push(std::move(task));
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::release_dynamic_pusher(int dynamic_pusher_id) -> void
{
	vsuc::insert(&dead_dynamic_pushers_, dynamic_pusher_id);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::make_pusher(size_t max_size) -> lock_free_task_pusher_static<LockFreeQueue>
{
	return lock_free_task_pusher_static(this, queues_.acquire(max_size));
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::push(clog::rcv_handle handle, lock_free_task task) -> void
{
	queues_.get(handle)->push(task);
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::release_static_pusher(clog::rcv_handle handle) -> void
{
	queues_.release(handle);
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::release_dynamic_pusher(clog::rcv_handle handle, int dynamic_pusher_id) -> void
{
	queues_.get(handle)->release_dynamic_pusher(dynamic_pusher_id);
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::next_dynamic_pusher_id() -> int
{
	return next_dynamic_pusher_id_++;
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::process_all() -> void
{
	for (const auto handle : queues_.active_handles())
	{
		queues_.get(handle)->process_all();
	}
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free pusher static
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline lock_free_task_pusher_static<LockFreeQueue>::lock_free_task_pusher_static(lock_free_task_pusher_static<LockFreeQueue>&& rhs) noexcept
	: processor_{rhs.processor_}
	, handle_{rhs.handle_}
{
	rhs.processor_ = {};
}

template <typename LockFreeQueue>
inline lock_free_task_pusher_static<LockFreeQueue>::lock_free_task_pusher_static(lock_free_task_processor<LockFreeQueue>* processor, clog::rcv_handle handle)
	: processor_{processor}
	, handle_{handle}
{
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_static<LockFreeQueue>::operator=(lock_free_task_pusher_static<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher_static<LockFreeQueue>&
{
	processor_ = rhs.processor_;
	handle_ = rhs.handle_;
	rhs.processor_ = {};

	return *this;
}

template <typename LockFreeQueue>
inline lock_free_task_pusher_static<LockFreeQueue>::~lock_free_task_pusher_static()
{
	if (!processor_) return;

	release();
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_static<LockFreeQueue>::make_pusher() -> lock_free_task_pusher_dynamic<LockFreeQueue>
{
	return {this, processor_->next_dynamic_pusher_id()};
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_static<LockFreeQueue>::push(task_t task) -> void
{
	if (!processor_) return;

	processor_->push(handle_, lock_free_task{-1, task});
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_static<LockFreeQueue>::push(lock_free_task task) -> void
{
	if (!processor_) return;

	processor_->push(handle_, task);
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_static<LockFreeQueue>::release() -> void
{
	processor_->release_static_pusher(handle_);

	processor_ = {};
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_static<LockFreeQueue>::release_dynamic_pusher(int dynamic_pusher_id) -> void
{
	if (!processor_) return;

	processor_->release_dynamic_pusher(handle_, dynamic_pusher_id);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free pusher dynamic
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline lock_free_task_pusher_dynamic<LockFreeQueue>::lock_free_task_pusher_dynamic(lock_free_task_pusher_dynamic<LockFreeQueue>&& rhs) noexcept
	: processor_{rhs.processor_}
	, dynamic_pusher_id{rhs.dynamic_pusher_id}
{
	rhs.processor_ = {};
}

template <typename LockFreeQueue>
inline lock_free_task_pusher_dynamic<LockFreeQueue>::lock_free_task_pusher_dynamic(lock_free_task_pusher_static<LockFreeQueue>* static_pusher, int dynamic_pusher_id)
	: static_pusher_{static_pusher}
	, dynamic_pusher_id{dynamic_pusher_id}
{
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_dynamic<LockFreeQueue>::operator=(lock_free_task_pusher_dynamic<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher_dynamic<LockFreeQueue>&
{
	static_pusher_ = rhs.static_pusher_;
	dynamic_pusher_id = rhs.dynamic_pusher_id;
	rhs.static_pusher_ = {};

	return *this;
}

template <typename LockFreeQueue>
inline lock_free_task_pusher_dynamic<LockFreeQueue>::~lock_free_task_pusher_dynamic()
{
	if (!static_pusher_) return;

	release();
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_dynamic<LockFreeQueue>::push(task_t task) -> void
{
	if (!static_pusher_) return;

	static_pusher_->push(lock_free_task{dynamic_pusher_id, task});
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher_dynamic<LockFreeQueue>::release() -> void
{
	static_pusher_->release_dynamic_pusher(dynamic_pusher_id);

	static_pusher_ = {};
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// locking processor queue
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline locking_task_processor::queue::queue(queue&& rhs) noexcept
	: queue_{std::move(rhs.queue_)}
{
}

inline auto locking_task_processor::queue::process_all() -> void
{
	std::unique_lock lock{mutex_};

	const auto queue{std::move(queue_)};

	queue_.clear();

	lock.unlock();

	for (const auto& task : queue)
	{
		task();
	}
}

inline auto locking_task_processor::queue::push(task_t task) -> void
{
	std::unique_lock lock{mutex_};

	queue_.push_back(std::move(task));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// locking processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto locking_task_processor::make_pusher() -> locking_task_pusher
{
	std::unique_lock lock{mutex_};

	const auto handle{queues_.acquire()};

	lock.unlock();

	return locking_task_pusher(this, handle);
}

inline auto locking_task_processor::push(clog::rcv_handle handle, task_t task) -> void
{
	std::unique_lock lock{mutex_};

	queues_.get(handle)->push(task);
}

inline auto locking_task_processor::release(clog::rcv_handle handle) -> void
{
	std::unique_lock lock{mutex_};

	queues_.release(handle);
}

inline auto locking_task_processor::process_all() -> void
{
	std::unique_lock lock{mutex_};

	for (const auto handle : queues_.active_handles())
	{
		queues_.get(handle)->process_all();
	}
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// locking pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline locking_task_pusher::locking_task_pusher(locking_task_pusher&& rhs) noexcept
	: processor_{rhs.processor_}
	, handle_{rhs.handle_}
{
	rhs.processor_ = {};
}

inline locking_task_pusher::locking_task_pusher(locking_task_processor* processor, clog::rcv_handle handle)
	: processor_{processor}
	, handle_{handle}
{
}

inline auto locking_task_pusher::operator=(locking_task_pusher&& rhs) noexcept -> locking_task_pusher&
{
	processor_ = rhs.processor_;
	handle_ = rhs.handle_;
	rhs.processor_ = {};

	return *this;
}

inline locking_task_pusher::~locking_task_pusher()
{
	if (!processor_) return;

	release();
}

inline auto locking_task_pusher::push(task_t task) -> void
{
	if (!processor_) return;

	processor_->push(handle_, task);
}

inline auto locking_task_pusher::release() -> void
{
	processor_->release(handle_);

	processor_ = {};
}

} // clog

#if defined(CLOG_WITH_MOODYCAMEL)

#include <readerwriterqueue.h>

namespace clog {

struct moodycamel_rwq
{
	moodycamel_rwq(size_t max_size)
		: impl_{max_size}
	{
	}

	inline auto pop(lock_free_task* out_task) -> bool
	{
		return impl_.try_dequeue(*out_task);
	}

	template <typename TaskT>
	inline auto push(TaskT&& task) -> void
	{
#	if _DEBUG
		const auto success{ impl_.try_emplace(std::move(task)) };

		assert(success);
#	else
		impl_.(std::move(task));
#	endif
	}

private:

	moodycamel::ReaderWriterQueue<lock_free_task> impl_;
};

using lock_free_task_processor_mc = lock_free_task_processor<moodycamel_rwq>;
using lock_free_task_pusher_static_mc = lock_free_task_pusher_static<moodycamel_rwq>;
using lock_free_task_pusher_dynamic_mc = lock_free_task_pusher_dynamic<moodycamel_rwq>;

} // clog

#endif // defined(CLOG_WITH_MOODYCAMEL)
