#pragma once

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "rcv.hpp"

#if defined(_DEBUG)
#include <iostream>
#endif

namespace clg {

using task_t = std::function<void()>;
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
// You can create more pushers while the producer thread is already pushing tasks,
// but you must call process_all() at least once before pushing any tasks with the
// newly created pusher.
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
template <typename LockFreeQueue>
class lock_free_task_processor
{
public:

	auto make_pusher(size_t initial_size) -> lock_free_task_pusher<LockFreeQueue>;
	auto process_all() -> void;

private:

	auto push(clg::rcv_handle handle, task_t task) -> void;
	auto release_pusher(clg::rcv_handle handle) -> void;

	struct queue
	{
		queue(size_t initial_size);
		queue(queue&&) noexcept = default;

		auto process_all() -> void;
		auto process_all(LockFreeQueue* q) -> void;
		auto push(task_t task) -> void;
		auto get_size() const { return size_; }

	private:

		size_t size_;
		std::array<LockFreeQueue, 2> queue_pair_;
		std::atomic<size_t> push_index_{0};
	};

	using queue_vector = clg::unsafe_rcv<std::unique_ptr<queue>>;

	bool overflow_{false};
	std::array<queue_vector, 2> queue_vec_pair_;
	std::atomic<size_t> push_index_{0};

	friend class lock_free_task_pusher<LockFreeQueue>;
};

template <typename LockFreeQueue>
class lock_free_task_pusher
{
public:

	lock_free_task_pusher() = default;
	lock_free_task_pusher(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept;
	lock_free_task_pusher(lock_free_task_processor<LockFreeQueue>* processor, clg::rcv_handle handle);
	auto operator=(lock_free_task_pusher<LockFreeQueue>&& rhs) noexcept -> lock_free_task_pusher<LockFreeQueue>&;
	~lock_free_task_pusher();

	auto push(task_t task) -> void;
	auto release() -> void;

private:

	lock_free_task_processor<LockFreeQueue>* processor_{};
	clg::rcv_handle handle_;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor queue
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline lock_free_task_processor<LockFreeQueue>::queue::queue(size_t initial_size)
	: size_{initial_size}
	, queue_pair_{LockFreeQueue{initial_size}, LockFreeQueue{}}
{
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::process_all(LockFreeQueue* q) -> void
{
	task_t task;

	while (q->pop(&task))
	{
		task();
	}
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::process_all() -> void
{
	if (queue_pair_[push_index_].get_size_approx() > size_ / 2)
	{
		size_ *= 2;
		queue_pair_[1 - push_index_] = LockFreeQueue{size_};
		push_index_ = 1 - push_index_;

#		if defined(_DEBUG)
			std::cout << "Queue size increased to " << size_ << "\n";
#		endif

		process_all(&queue_pair_[1 - push_index_]);
	}

	process_all(&queue_pair_[push_index_]);
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::queue::push(task_t task) -> void
{
	queue_pair_[push_index_].push(std::move(task));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::make_pusher(size_t initial_size) -> lock_free_task_pusher<LockFreeQueue>
{
	const size_t push_index{push_index_};

	if (!overflow_)
	{
		queue_vector& push_queue_vec{queue_vec_pair_[push_index]};

		if (push_queue_vec.size() < push_queue_vec.capacity())
		{
			return lock_free_task_pusher(this, push_queue_vec.acquire(new queue{initial_size}));
		}

		queue_vector& overflow_queue_vec{queue_vec_pair_[1 - push_index]};

		overflow_queue_vec.reserve(std::max(push_queue_vec.capacity(), size_t(1)) * 2);

		for (const auto handle : push_queue_vec.active_handles())
		{
			overflow_queue_vec.acquire_at(handle, new queue{(*push_queue_vec.get(handle))->get_size()});
		}

#		if defined(_DEBUG)
			std::cout << "Resized queue vector to " << overflow_queue_vec.capacity() << "\n";
#		endif

		overflow_ = true;

		return lock_free_task_pusher(this, overflow_queue_vec.acquire(new queue{initial_size}));
	}

	queue_vector& overflow_queue_vec{queue_vec_pair_[1 - push_index]};

	if (overflow_queue_vec.size() == overflow_queue_vec.capacity())
	{
		const queue_vector& push_queue_vec{queue_vec_pair_[push_index]};

		overflow_queue_vec.reserve(std::max(overflow_queue_vec.capacity(), size_t(1)) * 2);

#		if defined(_DEBUG)
			std::cout << "Resized queue vector to " << overflow_queue_vec.capacity() << "\n";
#		endif
	}

	return lock_free_task_pusher(this, overflow_queue_vec.acquire(new queue{initial_size}));
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::push(clg::rcv_handle handle, task_t task) -> void
{
	auto& queue_vec{queue_vec_pair_[push_index_]};

	(*queue_vec.get(handle))->push(task);
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::release_pusher(clg::rcv_handle handle) -> void
{
	const size_t push_index{push_index_};

	queue_vec_pair_[push_index].release(handle);

	if (overflow_)
	{
		queue_vec_pair_[1 - push_index].release(handle);
	}
}

template <typename LockFreeQueue>
inline auto lock_free_task_processor<LockFreeQueue>::process_all() -> void
{
	if (overflow_)
	{
		push_index_ = 1 - push_index_;

		auto& queue_vec{queue_vec_pair_[1 - push_index_]};

		for (const auto handle : queue_vec.active_handles())
		{
			(*queue_vec.get(handle))->process_all();
		}

		overflow_ = false;
	}

	auto& queue_vec{queue_vec_pair_[push_index_]};

	for (const auto handle : queue_vec.active_handles())
	{
		(*queue_vec.get(handle))->process_all();
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
inline lock_free_task_pusher<LockFreeQueue>::lock_free_task_pusher(lock_free_task_processor<LockFreeQueue>* processor, clg::rcv_handle handle)
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
	processor_->release_pusher(handle_);

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

	auto push(clg::rcv_handle handle, task_t task) -> void;
	auto release(clg::rcv_handle handle) -> void;

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

	auto push(task_t task) -> void;
	auto release() -> void;

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

	for (const auto task : queue)
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

inline auto locking_task_processor::push(clg::rcv_handle handle, task_t task) -> void
{
	std::unique_lock lock{mutex_};

	queues_.get(handle)->push(task);
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
	auto push(rcv_handle slot, task_t task) -> void;
	auto push(rcv_handle slot, task_t task, index_t index) -> void;
	auto release(rcv_handle slot) -> int;

	struct slot
	{
		auto clear() -> int;
		auto is_empty() const -> bool;
		auto process_all() -> int;
		auto push(task_t task) -> int;
		auto push(task_t task, index_t index) -> int;

	private:

		struct task_vector
		{
			auto clear() -> int;
			auto process_all() -> int;
			auto push(task_t task) -> int;
			auto push(task_t task, index_t index) -> int;

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

	auto push(task_t task) -> void;
	auto push(serial_task_processor::index_t index, task_t task) -> void;

	template <typename ConvertibleToindex_tType>
	auto push(ConvertibleToindex_tType index, task_t task) -> void
	{
		push(static_cast<serial_task_processor::index_t>(index), task);
	}

	template <typename ConvertibleToindex_tType>
	auto push(ConvertibleToindex_tType index) -> void
	{
		const auto index_conv { static_cast<serial_task_processor::index_t>(index) };

		assert (premapped_tasks_.find(index_conv) != std::cend(premapped_tasks_));

		push(index, premapped_tasks_[index_conv]);
	}

	auto release() -> void;

	template <typename ConvertibleToindex_tType>
	auto& operator[](ConvertibleToindex_tType index)
	{
		return premapped_tasks_[static_cast<serial_task_processor::index_t>(index)];
	}

	template <typename ConvertibleToIndex>
	auto operator<<(ConvertibleToIndex index) -> void
	{
		push(index);
	}

	template <typename ConvertibleToIndex>
	auto make_callable(ConvertibleToIndex index)
	{
		return [this, index]() { push(index); };
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

inline auto serial_task_processor::slot::task_vector::push(task_t task) -> int
{
	tasks_.push_back(task);

	return 1;
}

inline auto serial_task_processor::slot::task_vector::push(task_t task, index_t index) -> int
{
	if (indexed_tasks_.size() <= index)
	{
		indexed_tasks_.resize(index + 1);
	}

	if (indexed_tasks_[index]) return 0;

	indexed_tasks_[index] = task;
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

inline auto serial_task_processor::slot::push(task_t task) -> int
{
	int pushed_tasks{ 0 };

	if (processing_)
	{
		pushed_tasks = pushed_while_processing_.push(task);
	}
	else
	{
		pushed_tasks = tasks_.push(task);
	}

	total_tasks_ += pushed_tasks;

	return pushed_tasks;
}

inline auto serial_task_processor::slot::push(task_t task, index_t index) -> int
{
	int pushed_tasks{ 0 };

	if (processing_)
	{
		pushed_tasks = pushed_while_processing_.push(task, index);
	}
	else
	{
		pushed_tasks = tasks_.push(task, index);
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

inline auto serial_task_processor::push(rcv_handle handle, task_t task) -> void
{
	const auto slot{slots_.get(handle)};
	const auto was_empty{slot->is_empty()};

	total_tasks_ += slot->push(task);

	if (was_empty && !slot->is_empty())
	{
		busy_slots_.push_back(handle);
	}
}

inline auto serial_task_processor::push(rcv_handle handle, task_t task, index_t index) -> void
{
	const auto slot{slots_.get(handle)};
	const auto was_empty{slot->is_empty()};

	total_tasks_ += slot->push(task, index);

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

inline auto serial_task_pusher::push(task_t task) -> void
{
	if (!processor_) return;

	processor_->push(slot_, task);
}

inline auto serial_task_pusher::push(serial_task_processor::index_t index, task_t task) -> void
{
	if (!processor_) return;

	processor_->push(slot_, task, index);
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

	template <typename TaskT>
	inline auto push(TaskT&& task) -> void
	{
#	if _DEBUG
		const auto success{ impl_.try_emplace(std::move(task)) };

		assert(success);
#	else
		impl_.emplace(std::move(task));
#	endif
	}

private:

	moodycamel::ReaderWriterQueue<task_t> impl_;
};

using lock_free_task_processor_mc = lock_free_task_processor<moodycamel_rwq>;
using lock_free_task_pusher_mc = lock_free_task_pusher<moodycamel_rwq>;

} // clg

#endif // defined(CLOG_WITH_MOODYCAMEL)
