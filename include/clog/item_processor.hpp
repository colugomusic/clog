#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "stable_vector.hpp"

#if defined(_DEBUG)
#include <iostream>
#endif

namespace clg {
namespace q {

template <typename QueueImpl, typename AllocationPolicy> class lock_free_pusher;
template <typename T> class locking_pusher;
template <typename T> class serial_pusher;

////////////////////////////////////////////////////////////////////////////////////
// Lock-free ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//
// Single-consumer, single-producer item processing multi-queue.
// 
// You have to provide your own lock-free queue implementation. If you have
// github:cameron314/readerwriterqueue in your include paths and define
// CLOG_WITH_MOODYCAMEL then an implementation will be provided for you (at the
// bottom of this file.)
// 
// Process items by calling process_all() in the consumer thread.
// 
// To push items onto the queue you have to create a pusher. Create pushers in the
// consumer thread by calling make_pusher(initial_size), then call
// pusher.push(item) in the producer thread to push a item. Each pusher gets its
// own queue onto which it will push items.
//
// You can create more pushers while the producer thread is already pushing items.
// 
// Items are not processed if the pusher through which they were pushed goes out of
// scope. So it is ok to push a lambda which captures 'this' without having to
// worry about 'this' being deleted, if 'this' owns the pusher.
// 
// Memory can be allocated by either the consumer or producer, depending on the
// AllocationPolicy template argument. The possible allocation policies are:
//
// clg::q::may_allocate_on_process
//
//   The queue will double in size when process_all() is called, if it is already
//   at half-capacity.
//
//   Note that if you are not calling process_all() regularly enough then the
//   queue may still fill up. It is up to the queue implementation what happens
//   in this case.
//
//   In the provided moodycamel implementation, if _DEBUG is defined then pushing
//   a item onto a full queue is an assertion failure. If _DEBUG is not defined
//   then the producer thread will allocate memory for the item.
//
// clg::q::may_allocate_on_push
//
//   The queue may allocate more memory when push() is called.
// 
// clg::q::never_allocate
// 
//   The queue never allocates more memory
//

namespace detail {

template <typename QueueImpl>
struct lock_free_queue_may_allocate_on_process
{
	lock_free_queue_may_allocate_on_process(size_t initial_size);

	template <typename Processor>
	auto process_all(Processor&& processor) -> void;

	auto get_size() const { return size_; }

	template <typename T>
	auto push(T&& value) -> void;

private:

	template <typename Processor>
	auto process_all(Processor&& processor, QueueImpl* q) -> void;

	size_t size_;
	std::array<QueueImpl, 2> queue_pair_;
	std::atomic<size_t> push_index_{0};
};

template <typename QueueImpl>
struct lock_free_queue_basic
{
	lock_free_queue_basic(size_t initial_size)
		: queue_{initial_size}
	{
	}

	template <typename Processor>
	auto process_all(Processor&& processor) -> void
	{
		typename QueueImpl::value_type value;

		while (queue_.pop(&value))
		{
			processor(std::move(value));
		}
	}

protected:

	template <typename T>
	auto push_may_allocate(T&& value) -> void
	{
		queue_.push_may_allocate(std::forward<T>(value));
	}

	template <typename T>
	auto push_may_not_allocate(T&& value) -> void
	{
		queue_.push_may_not_allocate(std::forward<T>(value));
	}

private:

	QueueImpl queue_;
};

template <typename QueueImpl>
struct lock_free_queue_may_allocate_on_push : public lock_free_queue_basic<QueueImpl>
{
	lock_free_queue_may_allocate_on_push(size_t initial_size)
		: lock_free_queue_basic<QueueImpl>{initial_size}
	{
	}

	template <typename T>
	auto push(T&& value) -> void
	{
		lock_free_queue_basic<QueueImpl>::template push_may_allocate(std::forward<T>(value));
	}
};

template <typename QueueImpl>
struct lock_free_queue_never_allocate : public lock_free_queue_basic<QueueImpl>
{
	lock_free_queue_never_allocate(size_t initial_size)
		: lock_free_queue_basic<QueueImpl>{initial_size}
	{
	}

	template <typename T>
	auto push(T&& value) -> void
	{
		lock_free_queue_basic<QueueImpl>::template push_may_not_allocate(std::forward<T>(value));
	}
};

template <typename QueueImplWithAllocationWrapper>
struct lock_free_pusher_body
{
	QueueImplWithAllocationWrapper q;
	size_t index;

	lock_free_pusher_body(size_t index_, size_t initial_size)
		: index{index_}
		, q{initial_size}
	{
	}
};

} // detail

struct may_allocate_on_process
{
	template <typename QueueImpl>
	using pusher_body_t = detail::lock_free_pusher_body<detail::lock_free_queue_may_allocate_on_process<QueueImpl>>;
};

struct may_allocate_on_push
{
	template <typename QueueImpl>
	using pusher_body_t = detail::lock_free_pusher_body<detail::lock_free_queue_may_allocate_on_push<QueueImpl>>;
};

struct never_allocate
{
	template <typename QueueImpl>
	using pusher_body_t = detail::lock_free_pusher_body<detail::lock_free_queue_never_allocate<QueueImpl>>;
};

template <typename QueueImpl, typename AllocationPolicy>
class lock_free_processor
{
public:

	using pusher_body_t = typename AllocationPolicy::template pusher_body_t<QueueImpl>;
	using pusher_t = lock_free_pusher<QueueImpl, AllocationPolicy>;

	auto make_pusher(size_t initial_size) -> pusher_t;

	template <typename Processor>
	auto process_all(Processor&& processor) -> void;

private:

	auto release_pusher(size_t index) -> void;

	std::vector<std::unique_ptr<pusher_body_t>> pushers_;
	std::vector<std::unique_ptr<pusher_body_t>> deferred_add_;
	std::vector<size_t> deferred_remove_;
	bool processing_{false};

	friend class lock_free_pusher<QueueImpl, AllocationPolicy>;
};

template <typename QueueImpl, typename AllocationPolicy>
class lock_free_pusher
{
public:

	using body_t = typename AllocationPolicy::template pusher_body_t<QueueImpl>;

	lock_free_pusher() = default;
	lock_free_pusher(lock_free_pusher<QueueImpl, AllocationPolicy>&& rhs) noexcept;
	lock_free_pusher(lock_free_processor<QueueImpl, AllocationPolicy>* processor, body_t* body);
	auto operator=(lock_free_pusher<QueueImpl, AllocationPolicy>&& rhs) noexcept -> lock_free_pusher<QueueImpl, AllocationPolicy>&;
	~lock_free_pusher();
	auto release() -> void;

	template <typename T>
	auto push(T&& item) -> void;

private:

	lock_free_processor<QueueImpl, AllocationPolicy>* processor_{};
	body_t* body_{};
};

namespace detail {

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free queue (may allocate on process)
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename QueueImpl>
lock_free_queue_may_allocate_on_process<QueueImpl>::lock_free_queue_may_allocate_on_process(size_t initial_size)
	: size_{initial_size}
	, queue_pair_{QueueImpl{initial_size}, QueueImpl{}}
{
}

template <typename QueueImpl>
template <typename Processor>
auto lock_free_queue_may_allocate_on_process<QueueImpl>::process_all(Processor&& processor, QueueImpl* q) -> void
{
	typename QueueImpl::value_type value;

	while (q->pop(&value))
	{
		processor(std::move(value));
	}
}

template <typename QueueImpl>
template <typename Processor>
auto lock_free_queue_may_allocate_on_process<QueueImpl>::process_all(Processor&& processor) -> void
{
	size_t push_index{push_index_};

	if (queue_pair_[push_index].get_size_approx() > size_ / 2)
	{
		size_ *= 2;
		queue_pair_[1 - push_index] = QueueImpl{size_};
		push_index_ = 1 - push_index;
		push_index = 1 - push_index;

#		if defined(_DEBUG)
			std::cout << "Queue size increased to " << size_ << "\n";
#		endif

		process_all(std::forward<Processor>(processor), &queue_pair_[1 - push_index]);
	}

	process_all(std::forward<Processor>(processor), &queue_pair_[push_index]);
}

template <typename QueueImpl>
template <typename T>
auto lock_free_queue_may_allocate_on_process<QueueImpl>::push(T&& value) -> void
{
	queue_pair_[push_index_].push_may_not_allocate(std::forward<T>(value));
}

} // detail

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename QueueImpl, typename AllocationPolicy>
auto lock_free_processor<QueueImpl, AllocationPolicy>::make_pusher(size_t initial_size) -> pusher_t
{
	auto body{std::make_unique<pusher_body_t>(pushers_.size(), initial_size)};
	const auto ptr{body.get()};

	if (processing_)
	{
		deferred_add_.push_back(std::move(body));
	}
	else
	{
		pushers_.push_back(std::move(body));
	}

	return pusher_t(this, ptr);
}

template <typename QueueImpl, typename AllocationPolicy>
auto lock_free_processor<QueueImpl, AllocationPolicy>::release_pusher(size_t index) -> void
{
	if (processing_)
	{
		deferred_remove_.push_back(index);
	}
	else
	{
		pushers_.erase(pushers_.begin() + index);

		index = 0;

		for (const auto& pusher : pushers_)
		{
			pusher->index = index++;
		}
	}
}

template <typename QueueImpl, typename AllocationPolicy>
template <typename Processor>
auto lock_free_processor<QueueImpl, AllocationPolicy>::process_all(Processor&& processor) -> void
{
	processing_ = true;

	for (const auto& pusher : pushers_)
	{
		pusher->q.process_all(processor);
	}

	if (!deferred_remove_.empty())
	{
		for (size_t i = deferred_remove_.size() - 1; i >= 0; i--)
		{
			pushers_.erase(pushers_.begin() + deferred_remove_[i]);
		}

		size_t index = 0;

		for (const auto& pusher : pushers_)
		{
			pusher->index = index++;
		}
	}

	for (auto& pusher : deferred_add_)
	{
		pushers_.push_back(std::move(pusher));
	}

	deferred_add_.clear();
	deferred_remove_.clear();

	processing_ = false;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// lock-free pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename QueueImpl, typename AllocationPolicy>
lock_free_pusher<QueueImpl, AllocationPolicy>::lock_free_pusher(lock_free_pusher<QueueImpl, AllocationPolicy>&& rhs) noexcept
	: processor_{rhs.processor_}
	, body_{rhs.body_}
{
	rhs.processor_ = {};
}

template <typename QueueImpl, typename AllocationPolicy>
lock_free_pusher<QueueImpl, AllocationPolicy>::lock_free_pusher(lock_free_processor<QueueImpl, AllocationPolicy>* processor, body_t* body)
	: processor_{processor}
	, body_{body}
{
}

template <typename QueueImpl, typename AllocationPolicy>
auto lock_free_pusher<QueueImpl, AllocationPolicy>::operator=(lock_free_pusher<QueueImpl, AllocationPolicy>&& rhs) noexcept -> lock_free_pusher<QueueImpl, AllocationPolicy>&
{
	processor_ = rhs.processor_;
	body_ = rhs.body_;
	rhs.processor_ = {};

	return *this;
}

template <typename QueueImpl, typename AllocationPolicy>
lock_free_pusher<QueueImpl, AllocationPolicy>::~lock_free_pusher()
{
	if (!processor_) return;

	release();
}

template <typename QueueImpl, typename AllocationPolicy>
template <typename T>
auto lock_free_pusher<QueueImpl, AllocationPolicy>::push(T&& item) -> void
{
	if (!processor_) return;

	body_->q.push(std::forward<T>(item));
}

template <typename QueueImpl, typename AllocationPolicy>
auto lock_free_pusher<QueueImpl, AllocationPolicy>::release() -> void
{
	processor_->release_pusher(body_->index);

	processor_ = {};
}

////////////////////////////////////////////////////////////////////////////////////
// Locking /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class locking_processor
{
public:

	auto make_pusher() -> locking_pusher<T>;

	template <typename Processor>
	auto process_all(Processor&& processor) -> void;

private:

	auto release(uint32_t handle) -> void;

	template <typename U>
	auto push(uint32_t handle, U&& item) -> void;

	struct queue
	{
		queue() = default;
		queue(queue&& rhs) noexcept;

		template <typename Processor>
		auto process_all(Processor&& processor) -> void;

		template <typename U>
		auto push(U&& item) -> void;

	private:

		std::vector<T> queue_;
		std::mutex mutex_;
	};

	clg::stable_vector<queue> queues_;
	std::mutex mutex_;

	friend class locking_pusher<T>;
};

template <typename T>
class locking_pusher
{
public:

	locking_pusher() = default;
	locking_pusher(locking_pusher&& rhs) noexcept;
	locking_pusher(locking_processor<T>* processor, uint32_t handle);
	auto operator=(locking_pusher&& rhs) noexcept -> locking_pusher&;
	~locking_pusher();

	auto release() -> void;

	template <typename U>
	auto push(U&& item) -> void;

private:

	locking_processor<T>* processor_{};
	uint32_t handle_;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// locking processor queue
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline locking_processor<T>::queue::queue(queue&& rhs) noexcept
	: queue_{std::move(rhs.queue_)}
{
}

template <typename T>
template <typename Processor>
inline auto locking_processor<T>::queue::process_all(Processor&& processor) -> void
{
	std::unique_lock lock{mutex_};

	const auto queue{std::move(queue_)};

	queue_.clear();

	lock.unlock();

	for (auto& item : queue)
	{
		processor(std::move(item));
	}
}

template <typename T>
template <typename U>
inline auto locking_processor<T>::queue::push(U&& item) -> void
{
	std::unique_lock lock{mutex_};

	queue_.push_back(std::forward<U>(item));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// locking processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline auto locking_processor<T>::make_pusher() -> locking_pusher<T>
{
	std::unique_lock lock{mutex_};

	const auto handle{queues_.add(queue{})};

	lock.unlock();

	return locking_pusher(this, handle);
}

template <typename T>
template <typename U>
inline auto locking_processor<T>::push(uint32_t handle, U&& item) -> void
{
	std::unique_lock lock{mutex_};

	queues_[handle].push(std::forward<U>(item));
}

template <typename T>
inline auto locking_processor<T>::release(uint32_t handle) -> void
{
	std::unique_lock lock{mutex_};

	queues_.erase(handle);
}

template <typename T>
template <typename Processor>
inline auto locking_processor<T>::process_all(Processor&& processor) -> void
{
	std::unique_lock lock{mutex_};

	for (auto& queue : queues_)
	{
		queue.process_all(processor);
	}
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// locking pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline locking_pusher<T>::locking_pusher(locking_pusher<T>&& rhs) noexcept
	: processor_{rhs.processor_}
	, handle_{rhs.handle_}
{
	rhs.processor_ = {};
}

template <typename T>
inline locking_pusher<T>::locking_pusher(locking_processor<T>* processor, uint32_t handle)
	: processor_{processor}
	, handle_{handle}
{
}

template <typename T>
inline auto locking_pusher<T>::operator=(locking_pusher<T>&& rhs) noexcept -> locking_pusher<T>&
{
	processor_ = rhs.processor_;
	handle_ = rhs.handle_;
	rhs.processor_ = {};

	return *this;
}

template <typename T>
inline locking_pusher<T>::~locking_pusher()
{
	if (!processor_) return;

	release();
}

template <typename T>
template <typename U>
inline auto locking_pusher<T>::push(U&& item) -> void
{
	if (!processor_) return;

	processor_->push(handle_, std::forward<U>(item));
}

template <typename T>
inline auto locking_pusher<T>::release() -> void
{
	processor_->release(handle_);

	processor_ = {};
}

////////////////////////////////////////////////////////////////////////////////////
// Serial //////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class serial_processor
{
public:

	using index_t = size_t;

	auto make_pusher() -> serial_pusher<T>;

	template <typename Processor>
	auto process_all(Processor&& processor) -> void;

private:

	auto get_empty_slot() -> uint32_t;
	auto release(uint32_t slot) -> void;
	auto release_now(uint32_t slot) -> void;

	template <typename U>
	auto push(uint32_t slot, U&& item) -> void;

	template <typename U>
	auto push(uint32_t slot, U&& item, index_t index) -> void;

	struct slot
	{
		auto clear() -> int;
		auto is_empty() const -> bool;
		auto is_processing() const -> bool { return processing_; }

		template <typename Processor>
		auto process_all(Processor&& processor) -> int;

		template <typename U>
		auto push(U&& item) -> int;

		template <typename U>
		auto push(U&& item, index_t index) -> int;

	private:

		struct item_vector
		{
			auto clear() -> int;

			template <typename Processor>
			auto process_all(Processor&& processor) -> int;

			template <typename U>
			auto push(U&& item) -> int;

			template <typename U>
			auto push(U&& item, index_t index) -> int;

		private:

			std::vector<T> items_;
			std::vector<T> indexed_items_;
			std::vector<index_t> indices_;
		};

		bool processing_{ false };
		int total_items_{ 0 };
		item_vector items_;
		item_vector pushed_while_processing_;
	};

	clg::stable_vector<slot> slots_;
	std::vector<uint32_t> busy_slots_;
	std::vector<uint32_t> deferred_release_;
	int total_items_{ 0 };

	friend class serial_pusher<T>;
};

template <typename T>
class serial_pusher
{
public:

	serial_pusher() = default;
	serial_pusher(serial_pusher&& rhs) noexcept;
	serial_pusher(serial_processor<T>* processor, uint32_t slot);
	auto operator=(serial_pusher&& rhs) noexcept -> serial_pusher<T>&;
	~serial_pusher();

	template <typename U>
	auto push(U&& item) -> void;

	template <typename U>
	auto push_indexed(typename serial_processor<T>::index_t index, U&& item) -> void;

	template <typename ConvertibleToIndex, typename U>
	auto push_indexed(ConvertibleToIndex index, U&& item) -> void
	{
		push_indexed(static_cast<typename serial_processor<T>::index_t>(index), item);
	}

	template <typename ConvertibleToIndex>
	auto push_indexed(ConvertibleToIndex index) -> void
	{
		const auto index_conv { static_cast<typename serial_processor<T>::index_t>(index) };

		assert (premapped_items_.find(index_conv) != std::cend(premapped_items_));

		push_indexed(index, premapped_items_[index_conv]);
	}

	auto release() -> void;

	template <typename ConvertibleToIndex>
	auto& operator[](ConvertibleToIndex index)
	{
		return premapped_items_[static_cast<typename serial_processor<T>::index_t>(index)];
	}

	template <typename ConvertibleToIndex>
	auto operator<<(ConvertibleToIndex index) -> void
	{
		push_indexed(index);
	}

	template <typename ConvertibleToIndex>
	auto make_callable(ConvertibleToIndex index)
	{
		return [this, index]() { push_indexed(index); };
	}

private:

	serial_processor<T>* processor_{};
	uint32_t slot_;
	std::unordered_map<typename serial_processor<T>::index_t, T> premapped_items_;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial processor slot vector
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline auto serial_processor<T>::slot::item_vector::clear() -> int
{
	const auto out { items_.size() + indices_.size() };

	items_.clear();
	indexed_items_.clear();
	indices_.clear();

	return static_cast<int>(out);
}

template <typename T>
template <typename Processor>
inline auto serial_processor<T>::slot::item_vector::process_all(Processor&& processor) -> int
{
	for (auto& item : items_)
	{
		processor(std::move(item));
	}

	for (auto index : indices_)
	{
		processor(std::move(indexed_items_[index]));
	}

	return clear();
}

template <typename T>
template <typename U>
inline auto serial_processor<T>::slot::item_vector::push(U&& item) -> int
{
	items_.push_back(std::forward<U>(item));

	return 1;
}

template <typename T>
template <typename U>
inline auto serial_processor<T>::slot::item_vector::push(U&& item, index_t index) -> int
{
	if (indexed_items_.size() <= index)
	{
		indexed_items_.resize(index + 1);
	}

	if (indexed_items_[index]) return 0;

	indexed_items_[index] = std::forward<U>(item);
	indices_.push_back(index);

	return 1;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial processor slot
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline auto serial_processor<T>::slot::clear() -> int
{
	total_items_ = 0;

	return items_.clear() + pushed_while_processing_.clear();
}

template <typename T>
inline auto serial_processor<T>::slot::is_empty() const -> bool
{
	return total_items_ <= 0;
}

template <typename T>
template <typename Processor>
inline auto serial_processor<T>::slot::process_all(Processor&& processor) -> int
{
	processing_ = true;

	const auto total_processed { items_.process_all(std::forward<Processor>(processor)) };

	processing_ = false;

	items_ = std::move(pushed_while_processing_);

	pushed_while_processing_.clear();

	total_items_ -= total_processed;

	return total_processed;
}

template <typename T>
template <typename U>
inline auto serial_processor<T>::slot::push(U&& item) -> int
{
	int pushed_items{ 0 };

	if (processing_)
	{
		pushed_items = pushed_while_processing_.push(std::forward<U>(item));
	}
	else
	{
		pushed_items = items_.push(std::forward<U>(item));
	}

	total_items_ += pushed_items;

	return pushed_items;
}

template <typename T>
template <typename U>
inline auto serial_processor<T>::slot::push(U&& item, index_t index) -> int
{
	int pushed_items{ 0 };

	if (processing_)
	{
		pushed_items = pushed_while_processing_.push(std::forward<U>(item), index);
	}
	else
	{
		pushed_items = items_.push(std::forward<U>(item), index);
	}

	total_items_ += pushed_items;

	return pushed_items;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline auto serial_processor<T>::make_pusher() -> serial_pusher<T>
{
	return serial_pusher(this, slots_.add(slot{}));
}

template <typename T>
template <typename U>
inline auto serial_processor<T>::push(uint32_t handle, U&& item) -> void
{
	auto& slot{slots_[handle]};
	const auto was_empty{slot.is_empty()};

	total_items_ += slot.push(std::forward<U>(item));

	if (was_empty && !slot.is_empty())
	{
		busy_slots_.push_back(handle);
	}
}

template <typename T>
template <typename U>
inline auto serial_processor<T>::push(uint32_t handle, U&& item, index_t index) -> void
{
	auto& slot{slots_[handle]};
	const auto was_empty{slot.is_empty()};

	total_items_ += slot.push(std::forward<U>(item), index);

	if (was_empty && !slot.is_empty())
	{
		busy_slots_.push_back(handle);
	}
}

template <typename T>
inline auto serial_processor<T>::release(uint32_t handle) -> void
{
	const auto& slot{slots_[handle]};

	if (slot.is_processing())
	{
		deferred_release_.push_back(handle);
		return;
	}

	release_now(handle);
}

template <typename T>
inline auto serial_processor<T>::release_now(uint32_t handle) -> void
{
	auto& slot{slots_[handle]};
	const auto dropped_items{slot.clear()};

	total_items_ -= dropped_items;

	slots_.erase(handle);

	const auto pos{std::find(std::cbegin(busy_slots_), std::cend(busy_slots_), handle)};

	if (pos != std::cend(busy_slots_))
	{
		busy_slots_.erase(pos);
	}
}

template <typename T>
template <typename Processor>
inline auto serial_processor<T>::process_all(Processor&& processor) -> void
{
	while (total_items_ > 0)
	{
		assert (busy_slots_.size() > 0);

		const auto busy_slots{busy_slots_};

		for (auto handle : busy_slots)
		{
			auto& slot{slots_[handle]};

			if (slot.is_empty()) continue;

			total_items_ -= slot.process_all(processor);

			assert (total_items_ >= 0);

			if (total_items_ == 0) break;
		}
	}
	
	busy_slots_.clear();

	for (auto handle : deferred_release_)
	{
		release_now(handle);
	}

	deferred_release_.clear();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// serial pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline serial_pusher<T>::serial_pusher(serial_pusher<T>&& rhs) noexcept
	: processor_{rhs.processor_}
	, slot_{rhs.slot_}
{
	rhs.processor_ = {};
}

template <typename T>
inline serial_pusher<T>::serial_pusher(serial_processor<T>* processor, uint32_t slot)
	: processor_{ processor }
	, slot_{ slot }
{
}

template <typename T>
inline auto serial_pusher<T>::operator=(serial_pusher<T>&& rhs) noexcept -> serial_pusher<T>&
{
	processor_ = rhs.processor_;
	slot_ = rhs.slot_;
	rhs.processor_ = {};

	return *this;
}

template <typename T>
inline serial_pusher<T>::~serial_pusher()
{
	if (!processor_) return;

	release();
}

template <typename T>
template <typename U>
inline auto serial_pusher<T>::push(U&& item) -> void
{
	if (!processor_) return;

	processor_->push(slot_, std::forward<U>(item));
}

template <typename T>
template <typename U>
inline auto serial_pusher<T>::push_indexed(typename serial_processor<T>::index_t index, U&& item) -> void
{
	if (!processor_) return;

	processor_->push(slot_, std::forward<U>(item), index);
}

template <typename T>
inline auto serial_pusher<T>::release() -> void
{
	processor_->release(slot_);

	processor_ = {};
}

} // q
} // clg

#if defined(CLOG_WITH_MOODYCAMEL)

#include <readerwriterqueue.h>

namespace clg {
namespace q {

template <typename T>
struct moodycamel_rwq
{
	using value_type = T;

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

	inline auto pop(T* out_value) -> bool
	{
		return impl_.try_dequeue(*out_value);
	}

	template <typename U>
	inline auto push_may_not_allocate(U&& value) -> void
	{
#	if _DEBUG
		const auto success{ impl_.try_emplace(std::forward<U>(value)) };

		assert(success);
#	else
		impl_.emplace(std::forward<U>(value));
#	endif
	}

	template <typename U>
	inline auto push_may_allocate(U&& value) -> void
	{
		impl_.emplace(std::forward<U>(value));
	}

private:

	moodycamel::ReaderWriterQueue<T> impl_;
};

template <typename T, typename AllocationPolicy> using lock_free_processor_mc = lock_free_processor<moodycamel_rwq<T>, AllocationPolicy>;
template <typename T, typename AllocationPolicy> using lock_free_pusher_mc = lock_free_pusher<moodycamel_rwq<T>, AllocationPolicy>;

} // q
} // clg

#endif // defined(CLOG_WITH_MOODYCAMEL)
