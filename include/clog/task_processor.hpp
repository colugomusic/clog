#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include "rcv.hpp"
#include "small_function.hpp"

#if defined(_DEBUG)
#include <iostream>
#endif

namespace clg {

using task_t = small_function<void(), 512>;
template <typename LockFreeQueue> class lock_free_task_pusher;
class locking_task_pusher;
class serial_task_pusher;


////////////////////////////////////////////////////////////////////////////////////
// Lock-free ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//
// Single-consumer, single-producer task processing multi-queue.
// 
// You have to provide your own lock-free queue implementation. If you have
// github:cameron314/readerwriterqueue in your include paths and define
// CLOG_WITH_MOODYCAMEL then an implementation will be provided for you (at the
// bottom of this file.)
// 
// The consumer allocates memory and the producer does not.
//
// Process tasks by calling process_all() in the consumer thread.
// 
// To push tasks onto the queue you have to create a pusher. Create pushers in the
// consumer thread by calling make_pusher(initial_size), then call
// pusher.push(task) in the producer thread to push a task. Each pusher gets its
// own queue onto which it will push tasks.
//
// You can create more pushers while the producer thread is already pushing tasks.
// 
// Tasks are not processed if the pusher through which they were pushed goes out of
// scope. So it is straightforward to push a lambda which captures 'this' without
// having to worry about 'this' being deleted.
//
// Pusher queues will automatically double in size if they reach half-capacity.
// This happens in the consumer thread when you call process_all().
// 
// The point of the doubling-at-half-capacity idea is to prevent the pusher queues
// from ever filling up, but if you are not calling process_all() regularly enough
// then it could still happen! It is up to the queue implementation what happens
// in this case.
// 
// In the provided moodycamel implementation, if _DEBUG is defined then pushing
// a task onto a full queue is an assertion failure. If _DEBUG is not defined then
// the producer thread will allocate memory for the task (bad!)
// 

namespace detail {

template <typename LockFreeQueue>
struct lock_free_growing_queue
{
	lock_free_growing_queue(size_t initial_size);

	auto process_all() -> void;
	auto process_all(LockFreeQueue* q) -> void;
	auto get_size() const { return size_; }

	template <typename Task>
	auto push(Task&& task) -> void;

private:

	size_t size_;
	std::array<LockFreeQueue, 2> queue_pair_;
	std::atomic<size_t> push_index_{0};
};

template <typename LockFreeQueue>
struct lock_free_pusher_body
{
	lock_free_growing_queue<LockFreeQueue> q;
	size_t index;

	lock_free_pusher_body(size_t index_, size_t initial_size)
		: index{index_}
		, q{initial_size}
	{
	}
};

} // detail

template <typename LockFreeQueue>
class lock_free_task_processor
{
public:

	auto make_pusher(size_t initial_size) -> lock_free_task_pusher<LockFreeQueue>;
	auto process_all() -> void;

private:

	auto release_pusher(size_t index) -> void;

	std::vector<std::unique_ptr<detail::lock_free_pusher_body<LockFreeQueue>>> pushers_;

	friend class lock_free_task_pusher<LockFreeQueue>;
};

template <typename LockFreeQueue>
class lock_free_task_pusher
{
public:

	lock_free_task_pusher() = default;
	lock_free_task_pusher(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept;
	lock_free_task_pusher(lock_free_task_processor<LockFreeQueue>* processor, detail::lock_free_pusher_body<LockFreeQueue>* body);
	auto operator=(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher<LockFreeQueue>&;
	~lock_free_task_pusher();
	auto release() -> void;

	template <typename Task>
	auto push(Task&& task) -> void;

private:

	lock_free_task_processor<LockFreeQueue>* processor_{};
	detail::lock_free_pusher_body<LockFreeQueue>* body_{};
};

namespace detail {

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free growing queue
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
lock_free_growing_queue<LockFreeQueue>::lock_free_growing_queue(size_t initial_size)
	: size_{initial_size}
	, queue_pair_{LockFreeQueue{initial_size}, LockFreeQueue{}}
{
}

template <typename LockFreeQueue>
auto lock_free_growing_queue<LockFreeQueue>::process_all(LockFreeQueue* q) -> void
{
	task_t task;

	while (q->pop(&task))
	{
		task();
	}
}

template <typename LockFreeQueue>
auto lock_free_growing_queue<LockFreeQueue>::process_all() -> void
{
	size_t push_index{push_index_};

	if (queue_pair_[push_index].get_size_approx() > size_ / 2)
	{
		size_ *= 2;
		queue_pair_[1 - push_index] = LockFreeQueue{size_};
		push_index_ = 1 - push_index;
		push_index = 1 - push_index;

#		if defined(_DEBUG)
			std::cout << "Queue size increased to " << size_ << "\n";
#		endif

		process_all(&queue_pair_[1 - push_index]);
	}

	process_all(&queue_pair_[push_index]);
}

template <typename LockFreeQueue>
template <typename Task>
auto lock_free_growing_queue<LockFreeQueue>::push(Task&& task) -> void
{
	queue_pair_[push_index_].push(std::forward<Task>(task));
}

} // detail

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
auto lock_free_task_processor<LockFreeQueue>::make_pusher(size_t initial_size) -> lock_free_task_pusher<LockFreeQueue>
{
	auto body{std::make_unique<detail::lock_free_pusher_body<LockFreeQueue>>(pushers_.size(), initial_size)};
	const auto ptr{body.get()};

	pushers_.push_back(std::move(body));

	return lock_free_task_pusher<LockFreeQueue>(this, ptr);
}

template <typename LockFreeQueue>
auto lock_free_task_processor<LockFreeQueue>::release_pusher(size_t index) -> void
{
	pushers_.erase(pushers_.begin() + index);

	index = 0;

	for (const auto& pusher : pushers_)
	{
		pusher->index = index++;
	}
}

template <typename LockFreeQueue>
auto lock_free_task_processor<LockFreeQueue>::process_all() -> void
{
	for (const auto& pusher : pushers_)
	{
		pusher->q.process_all();
	}
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
lock_free_task_pusher<LockFreeQueue>::lock_free_task_pusher(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept
	: processor_{rhs.processor_}
	, body_{rhs.body_}
{
	rhs.processor_ = {};
}

template <typename LockFreeQueue>
lock_free_task_pusher<LockFreeQueue>::lock_free_task_pusher(lock_free_task_processor<LockFreeQueue>* processor, detail::lock_free_pusher_body<LockFreeQueue>* body)
	: processor_{processor}
	, body_{body}
{
}

template <typename LockFreeQueue>
auto lock_free_task_pusher<LockFreeQueue>::operator=(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher<LockFreeQueue>&
{
	processor_ = rhs.processor_;
	body_ = rhs.body_;
	rhs.processor_ = {};

	return *this;
}

template <typename LockFreeQueue>
lock_free_task_pusher<LockFreeQueue>::~lock_free_task_pusher()
{
	if (!processor_) return;

	release();
}

template <typename LockFreeQueue>
template <typename Task>
auto lock_free_task_pusher<LockFreeQueue>::push(Task&& task) -> void
{
	if (!processor_) return;

	body_->q.push(std::forward<Task>(task));
}

template <typename LockFreeQueue>
auto lock_free_task_pusher<LockFreeQueue>::release() -> void
{
	processor_->release_pusher(body_->index);

	processor_ = {};
}

////////////////////////////////////////////////////////////////////////////////////
// Locking /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

class locking_task_processor
{
public:

	auto make_pusher() -> locking_task_pusher;
	auto process_all() -> void;

private:

	auto release(clg::rcv_handle handle) -> void;

	template <typename Task>
	auto push(clg::rcv_handle handle, Task&& task) -> void;

	struct queue
	{
		queue() = default;
		queue(queue&& rhs) noexcept;

		auto process_all() -> void;

		template <typename Task>
		auto push(Task&& task) -> void;

	private:

		std::vector<task_t> queue_;
		std::mutex mutex_;
	};

	clg::unsafe_rcv<queue> queues_;
	std::mutex mutex_;

	friend class locking_task_pusher;
};

class locking_task_pusher
{
public:

	locking_task_pusher() = default;
	locking_task_pusher(locking_task_pusher&& rhs) noexcept;
	locking_task_pusher(locking_task_processor* processor, clg::rcv_handle handle);
	auto operator=(locking_task_pusher&& rhs) noexcept -> locking_task_pusher&;
	~locking_task_pusher();

	auto release() -> void;

	template <typename Task>
	auto push(Task&& task) -> void;

private:

	locking_task_processor* processor_{};
	clg::rcv_handle handle_;
};

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

template <typename Task>
inline auto locking_task_processor::queue::push(Task&& task) -> void
{
	std::unique_lock lock{mutex_};

	queue_.push_back(std::forward<Task>(task));
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

template <typename Task>
inline auto locking_task_processor::push(clg::rcv_handle handle, Task&& task) -> void
{
	std::unique_lock lock{mutex_};

	queues_.get(handle)->push(std::forward<Task>(task));
}

inline auto locking_task_processor::release(clg::rcv_handle handle) -> void
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

inline locking_task_pusher::locking_task_pusher(locking_task_processor* processor, clg::rcv_handle handle)
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

template <typename Task>
inline auto locking_task_pusher::push(Task&& task) -> void
{
	if (!processor_) return;

	processor_->push(handle_, std::forward<Task>(task));
}

inline auto locking_task_pusher::release() -> void
{
	processor_->release(handle_);

	processor_ = {};
}

////////////////////////////////////////////////////////////////////////////////////
// Serial //////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

class serial_task_processor
{
public:

	using index_t = size_t;

	auto make_pusher() -> serial_task_pusher;
	auto process_all() -> void;

private:

	auto get_empty_slot() -> rcv_handle;
	auto release(rcv_handle slot) -> int;

	template <typename Task>
	auto push(rcv_handle slot, Task&& task) -> void;

	template <typename Task>
	auto push(rcv_handle slot, Task&& task, index_t index) -> void;

	struct slot
	{
		auto clear() -> int;
		auto is_empty() const -> bool;
		auto process_all() -> int;

		template <typename Task>
		auto push(Task&& task) -> int;

		template <typename Task>
		auto push(Task&& task, index_t index) -> int;

	private:

		struct task_vector
		{
			auto clear() -> int;
			auto process_all() -> int;

			template <typename Task>
			auto push(Task&& task) -> int;

			template <typename Task>
			auto push(Task&& task, index_t index) -> int;

		private:

			std::vector<task_t> tasks_;
			std::vector<task_t> indexed_tasks_;
			std::vector<index_t> indices_;
		};

		bool processing_{ false };
		int total_tasks_{ 0 };
		task_vector tasks_;
		task_vector pushed_while_processing_;
	};

	clg::unsafe_rcv<slot> slots_;
	std::vector<rcv_handle> busy_slots_;
	int total_tasks_{ 0 };

	friend class serial_task_pusher;
};

class serial_task_pusher
{
public:

	serial_task_pusher() = default;
	serial_task_pusher(serial_task_pusher&& rhs) noexcept;
	serial_task_pusher(serial_task_processor* processor, rcv_handle slot);
	auto operator=(serial_task_pusher&& rhs) noexcept -> serial_task_pusher&;
	~serial_task_pusher();

	template <typename Task>
	auto push_task(Task&& task) -> void;

	template <typename Task>
	auto push_indexed_task(serial_task_processor::index_t index, Task&& task) -> void;

	template <typename ConvertibleToIndex, typename Task>
	auto push_indexed_task(ConvertibleToIndex index, Task&& task) -> void
	{
		push_indexed_task(static_cast<serial_task_processor::index_t>(index), task);
	}

	template <typename ConvertibleToIndex>
	auto push_indexed_task(ConvertibleToIndex index) -> void
	{
		const auto index_conv { static_cast<serial_task_processor::index_t>(index) };

		assert (premapped_tasks_.find(index_conv) != std::cend(premapped_tasks_));

		push_indexed_task(index, premapped_tasks_[index_conv]);
	}

	auto release() -> void;

	template <typename ConvertibleToIndex>
	auto& operator[](ConvertibleToIndex index)
	{
		return premapped_tasks_[static_cast<serial_task_processor::index_t>(index)];
	}

	template <typename ConvertibleToIndex>
	auto operator<<(ConvertibleToIndex index) -> void
	{
		push_indexed_task(index);
	}

	template <typename ConvertibleToIndex>
	auto make_callable(ConvertibleToIndex index)
	{
		return [this, index]() { push_indexed_task(index); };
	}

private:

	serial_task_processor* processor_{};
	rcv_handle slot_;
	std::unordered_map<serial_task_processor::index_t, task_t> premapped_tasks_;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial processor slot task vector
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto serial_task_processor::slot::task_vector::clear() -> int
{
	const auto out { tasks_.size() + indices_.size() };

	tasks_.clear();
	indexed_tasks_.clear();
	indices_.clear();

	return static_cast<int>(out);
}

inline auto serial_task_processor::slot::task_vector::process_all() -> int
{
	for (auto task : tasks_)
	{
		task();
	}

	for (auto index : indices_)
	{
		indexed_tasks_[index]();
	}

	return clear();
}

template <typename Task>
inline auto serial_task_processor::slot::task_vector::push(Task&& task) -> int
{
	tasks_.push_back(std::forward<Task>(task));

	return 1;
}

template <typename Task>
inline auto serial_task_processor::slot::task_vector::push(Task&& task, index_t index) -> int
{
	if (indexed_tasks_.size() <= index)
	{
		indexed_tasks_.resize(index + 1);
	}

	if (indexed_tasks_[index]) return 0;

	indexed_tasks_[index] = std::forward<Task>(task);
	indices_.push_back(index);

	return 1;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial processor slot
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto serial_task_processor::slot::clear() -> int
{
	total_tasks_ = 0;

	return tasks_.clear() + pushed_while_processing_.clear();
}

inline auto serial_task_processor::slot::is_empty() const -> bool
{
	return total_tasks_ <= 0;
}

inline auto serial_task_processor::slot::process_all() -> int
{
	processing_ = true;

	const auto total_processed { tasks_.process_all() };

	processing_ = false;

	tasks_ = std::move(pushed_while_processing_);

	pushed_while_processing_.clear();

	total_tasks_ -= total_processed;

	return total_processed;
}

template <typename Task>
inline auto serial_task_processor::slot::push(Task&& task) -> int
{
	int pushed_tasks{ 0 };

	if (processing_)
	{
		pushed_tasks = pushed_while_processing_.push(std::forward<Task>(task));
	}
	else
	{
		pushed_tasks = tasks_.push(std::forward<Task>(task));
	}

	total_tasks_ += pushed_tasks;

	return pushed_tasks;
}

template <typename Task>
inline auto serial_task_processor::slot::push(Task&& task, index_t index) -> int
{
	int pushed_tasks{ 0 };

	if (processing_)
	{
		pushed_tasks = pushed_while_processing_.push(std::forward<Task>(task), index);
	}
	else
	{
		pushed_tasks = tasks_.push(std::forward<Task>(task), index);
	}

	total_tasks_ += pushed_tasks;

	return pushed_tasks;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto serial_task_processor::make_pusher() -> serial_task_pusher
{
	return serial_task_pusher(this, slots_.acquire());
}

template <typename Task>
inline auto serial_task_processor::push(rcv_handle handle, Task&& task) -> void
{
	const auto slot{slots_.get(handle)};
	const auto was_empty{slot->is_empty()};

	total_tasks_ += slot->push(std::forward<Task>(task));

	if (was_empty && !slot->is_empty())
	{
		busy_slots_.push_back(handle);
	}
}

template <typename Task>
inline auto serial_task_processor::push(rcv_handle handle, Task&& task, index_t index) -> void
{
	const auto slot{slots_.get(handle)};
	const auto was_empty{slot->is_empty()};

	total_tasks_ += slot->push(std::forward<Task>(task), index);

	if (was_empty && !slot->is_empty())
	{
		busy_slots_.push_back(handle);
	}
}

inline auto serial_task_processor::release(rcv_handle handle) -> int
{
	const auto slot{slots_.get(handle)};
	const auto dropped_tasks{slot->clear()};

	total_tasks_ -= dropped_tasks;

	slots_.release(handle);

	const auto pos{std::find(std::cbegin(busy_slots_), std::cend(busy_slots_), handle)};

	if (pos != std::cend(busy_slots_))
	{
		busy_slots_.erase(pos);
	}

	return dropped_tasks;
}

inline auto serial_task_processor::process_all() -> void
{
	while (total_tasks_ > 0)
	{
		assert (busy_slots_.size() > 0);

		const auto busy_slots{busy_slots_};

		for (auto handle : busy_slots)
		{
			auto slot{slots_.get(handle)};

			if (slot->is_empty()) continue;

			total_tasks_ -= slot->process_all();

			assert (total_tasks_ >= 0);

			if (total_tasks_ == 0) break;
		}
	}
	
	busy_slots_.clear();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline serial_task_pusher::serial_task_pusher(serial_task_pusher&& rhs) noexcept
	: processor_{rhs.processor_}
	, slot_{rhs.slot_}
{
	rhs.processor_ = {};
}

inline serial_task_pusher::serial_task_pusher(serial_task_processor* processor, rcv_handle slot)
	: processor_{ processor }
	, slot_{ slot }
{
}

inline auto serial_task_pusher::operator=(serial_task_pusher&& rhs) noexcept -> serial_task_pusher&
{
	processor_ = rhs.processor_;
	slot_ = rhs.slot_;
	rhs.processor_ = {};

	return *this;
}

inline serial_task_pusher::~serial_task_pusher()
{
	if (!processor_) return;

	release();
}

template <typename Task>
inline auto serial_task_pusher::push_task(Task&& task) -> void
{
	if (!processor_) return;

	processor_->push(slot_, std::forward<Task>(task));
}

template <typename Task>
inline auto serial_task_pusher::push_indexed_task(serial_task_processor::index_t index, Task&& task) -> void
{
	if (!processor_) return;

	processor_->push(slot_, std::forward<Task>(task), index);
}

inline auto serial_task_pusher::release() -> void
{
	processor_->release(slot_);

	processor_ = {};
}

} // clg

#if defined(CLOG_WITH_MOODYCAMEL)

#include <readerwriterqueue.h>

namespace clg {

struct moodycamel_rwq
{
	moodycamel_rwq()
		: impl_{2}
	{
	}

	moodycamel_rwq(size_t max_size)
		: impl_{max_size}
	{
	}

	inline auto get_size_approx() const -> size_t
	{
		return impl_.size_approx();
	}

	inline auto pop(task_t* out_task) -> bool
	{
		return impl_.try_dequeue(*out_task);
	}

	template <typename Task>
	inline auto push(Task&& task) -> void
	{
#	if _DEBUG
		const auto success{ impl_.try_emplace(std::forward<Task>(task)) };

		assert(success);
#	else
		impl_.emplace(std::forward<Task>(task));
#	endif
	}

private:

	moodycamel::ReaderWriterQueue<task_t> impl_;
};

using lock_free_task_processor_mc = lock_free_task_processor<moodycamel_rwq>;
using lock_free_task_pusher_mc = lock_free_task_pusher<moodycamel_rwq>;

} // clg

#endif // defined(CLOG_WITH_MOODYCAMEL)
