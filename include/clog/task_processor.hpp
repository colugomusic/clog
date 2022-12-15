#pragma once

#include <functional>
#include <mutex>
#include "rcv.hpp"

namespace clog {

using task_t = std::function<void()>;
template <typename LockFreeQueue> class lock_free_task_pusher;

template <typename LockFreeQueue>
class lock_free_task_processor
{
public:

	//
	// Do not create new task pushers while the audio stream is active!
	// It's not thread safe!
	// 
	// Create all required task pushers before the audio stream starts
	//
	auto make_pusher(size_t max_size) -> lock_free_task_pusher<LockFreeQueue>;
	auto process_all() -> void;

private:

	auto push(clog::rcv_handle handle, task_t task) -> void;
	auto release(clog::rcv_handle handle) -> void;

	struct queue
	{
		queue(size_t max_size);
		queue(queue&&) noexcept = default;

		auto process_all() -> void;
		auto push(task_t task) -> void;

	private:

		LockFreeQueue queue_;
	};

	clog::unsafe_rcv<queue> queues_;

	friend class lock_free_task_pusher<LockFreeQueue>;
};

template <typename LockFreeQueue>
class lock_free_task_pusher
{
public:

	lock_free_task_pusher() = default;
	lock_free_task_pusher(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept;
	lock_free_task_pusher(lock_free_task_processor<LockFreeQueue>* processor, clog::rcv_handle handle);
	auto operator=(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher<LockFreeQueue>&;
	~lock_free_task_pusher();

	auto push(task_t task) -> void;
	auto release() -> void;

private:

	lock_free_task_processor<LockFreeQueue>* processor_{};
	clog::rcv_handle handle_;
};

class locking_task_pusher;

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
	task_t task;

	while (queue_.pop(&task))
	{
		task();
	}
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::push(task_t task) -> void
{
	queue_.push(std::move(task));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::make_pusher(size_t max_size) -> lock_free_task_pusher<LockFreeQueue>
{
	return lock_free_task_pusher(this, queues_.acquire(max_size));
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::push(clog::rcv_handle handle, task_t task) -> void
{
	queues_.get(handle)->push(task);
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::release(clog::rcv_handle handle) -> void
{
	queues_.release(handle);
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
// lock-free pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline lock_free_task_pusher<LockFreeQueue>::lock_free_task_pusher(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept
	: processor_{rhs.processor_}
	, handle_{rhs.handle_}
{
	rhs.processor_ = {};
}

template <typename LockFreeQueue>
inline lock_free_task_pusher<LockFreeQueue>::lock_free_task_pusher(lock_free_task_processor<LockFreeQueue>* processor, clog::rcv_handle handle)
	: processor_{processor}
	, handle_{handle}
{
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher<LockFreeQueue>::operator=(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher<LockFreeQueue>&
{
	processor_ = rhs.processor_;
	handle_ = rhs.handle_;
	rhs.processor_ = {};

	return *this;
}

template <typename LockFreeQueue>
inline lock_free_task_pusher<LockFreeQueue>::~lock_free_task_pusher()
{
	if (!processor_) return;

	release();
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher<LockFreeQueue>::push(task_t task) -> void
{
	if (!processor_) return;

	processor_->push(handle_, task);
}

template <typename LockFreeQueue>
inline auto lock_free_task_pusher<LockFreeQueue>::release() -> void
{
	processor_->release(handle_);

	processor_ = {};
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

	inline auto pop(task_t* out_task) -> bool
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

	moodycamel::ReaderWriterQueue<task_t> impl_;
};

using lock_free_task_processor_mc = lock_free_task_processor<moodycamel_rwq>;
using lock_free_task_pusher_mc = lock_free_task_pusher<moodycamel_rwq>;

} // clog

#endif // defined(CLOG_WITH_MOODYCAMEL)