#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <vector>

#if defined(_DEBUG) && 1
#include <iostream>
#endif

namespace clog {

using idle_task = std::function<void()>;
class idle_task_pusher;

class idle_task_processor
{
public:

	using index_t = size_t;

	auto make_pusher() -> idle_task_pusher;
	auto process_all() -> void;

private:

	auto get_empty_slot() -> index_t;
	auto push(index_t slot, idle_task task) -> void;
	auto push(index_t slot, idle_task task, index_t index) -> void;
	auto release(index_t slot) -> int;

	struct slot
	{
		bool occupied{ false };

		auto clear() -> int;
		auto is_empty() const -> bool;
		auto process_all() -> int;
		auto push(idle_task task) -> int;
		auto push(idle_task task, index_t index) -> int;

	private:

		struct task_vector
		{
			auto clear() -> int;
			auto process_all() -> int;
			auto push(idle_task task) -> int;
			auto push(idle_task task, index_t index) -> int;

		private:

			std::vector<idle_task> tasks_;
			std::vector<idle_task> indexed_tasks_;
			std::vector<index_t> indices_;
		};

		bool processing_{ false };
		int total_tasks_{ 0 };
		task_vector tasks_;
		task_vector pushed_while_processing_;
	};

	std::vector<slot> slots_;
	std::vector<index_t> busy_slots_;
	index_t next_empty_slot_{ 0 };
	int total_tasks_{ 0 };

	friend class idle_task_pusher;
};

class idle_task_pusher
{
public:

	idle_task_pusher(idle_task_processor* processor, idle_task_processor::index_t slot);
	~idle_task_pusher();

	auto push(idle_task task) -> void;
	auto push(idle_task_processor::index_t index, idle_task task) -> void;

	template <typename ConvertibleToindex_tType>
	auto push(ConvertibleToindex_tType index, idle_task task) -> void
	{
		push(static_cast<idle_task_processor::index_t>(index), task);
	}

	template <typename ConvertibleToindex_tType>
	auto push(ConvertibleToindex_tType index) -> void
	{
		const auto index_conv { static_cast<idle_task_processor::index_t>(index) };

		assert (premapped_tasks_.find(index_conv) != std::cend(premapped_tasks_));

		push(index, premapped_tasks_[index_conv]);
	}

	auto release() -> void;

	template <typename ConvertibleToindex_tType>
	auto& operator[](ConvertibleToindex_tType index)
	{
		return premapped_tasks_[static_cast<idle_task_processor::index_t>(index)];
	}

	template <typename ConvertibleToIndex>
	auto operator<<(ConvertibleToIndex index) -> void
	{
		push(index);
	}

private:

	idle_task_processor* processor_;
	idle_task_processor::index_t slot_;
	std::unordered_map<idle_task_processor::index_t, idle_task> premapped_tasks_;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// processor slot task vector
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto idle_task_processor::slot::task_vector::clear() -> int
{
	const auto out { tasks_.size() + indices_.size() };

	tasks_.clear();
	indexed_tasks_.clear();
	indices_.clear();

	return static_cast<int>(out);
}

inline auto idle_task_processor::slot::task_vector::process_all() -> int
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

inline auto idle_task_processor::slot::task_vector::push(idle_task task) -> int
{
	tasks_.push_back(task);

	return 1;
}

inline auto idle_task_processor::slot::task_vector::push(idle_task task, index_t index) -> int
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
// processor slot
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto idle_task_processor::slot::clear() -> int
{
	total_tasks_ = 0;

	return tasks_.clear() + pushed_while_processing_.clear();
}

inline auto idle_task_processor::slot::is_empty() const -> bool
{
	return total_tasks_ <= 0;
}

inline auto idle_task_processor::slot::process_all() -> int
{
	processing_ = true;

	const auto total_processed { tasks_.process_all() };

	processing_ = false;

	tasks_ = std::move(pushed_while_processing_);

	pushed_while_processing_.clear();

	total_tasks_ -= total_processed;

	return total_processed;
}

inline auto idle_task_processor::slot::push(idle_task task) -> int
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

inline auto idle_task_processor::slot::push(idle_task task, index_t index) -> int
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
// processor
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline auto idle_task_processor::make_pusher() -> idle_task_pusher
{
	const auto slot { get_empty_slot() };

	if (slot >= slots_.size())
	{
		slots_.resize((slot + 1) * 2);

#if defined(_DEBUG) && 1
		std::cout << "Total idle task slots: " << slots_.size() << "\n";
#endif
	}

	slots_[slot].occupied = true;

	return idle_task_pusher(this, slot);
}

inline auto idle_task_processor::get_empty_slot() -> index_t
{
	const auto out { next_empty_slot_++ };

	while (true)
	{
		if (next_empty_slot_ >= slots_.size()) break;
		if (!slots_[next_empty_slot_].occupied) break;

		next_empty_slot_++;
	}

	return out;
}

inline auto idle_task_processor::push(index_t slot, idle_task task) -> void
{
	const auto was_empty { slots_[slot].is_empty() };

	total_tasks_ += slots_[slot].push(task);

	if (was_empty && !slots_[slot].is_empty())
	{
		busy_slots_.push_back(slot);
	}
}

inline auto idle_task_processor::push(index_t slot, idle_task task, index_t index) -> void
{
	const auto was_empty { slots_[slot].is_empty() };

	total_tasks_ += slots_[slot].push(task, index);

	if (was_empty && !slots_[slot].is_empty())
	{
		busy_slots_.push_back(slot);
	}
}

inline auto idle_task_processor::release(index_t slot) -> int
{
	const auto dropped_tasks { slots_[slot].clear() };

	total_tasks_ -= dropped_tasks;

	slots_[slot].occupied = false;
	
	if (slot < next_empty_slot_)
	{
		next_empty_slot_ = slot;
	}

	return dropped_tasks;
}

inline auto idle_task_processor::process_all() -> void
{
	while (total_tasks_ > 0)
	{
		assert (busy_slots_.size() > 0);

		const auto busy_slots { busy_slots_ };

		for (auto index : busy_slots)
		{
			auto& slot { slots_[index] };

			if (!slot.occupied) continue;
			if (slot.is_empty()) continue;

			total_tasks_ -= slot.process_all();

			assert (total_tasks_ >= 0);

			if (total_tasks_ == 0) break;
		}
	}
	
	busy_slots_.clear();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
// pusher
//++++++++++++++++++++++++++++++++++++++++++++++++++++
inline idle_task_pusher::idle_task_pusher(idle_task_processor* processor, idle_task_processor::index_t slot)
	: processor_{ processor }
	, slot_{ slot }
{
}

inline idle_task_pusher::~idle_task_pusher()
{
	if (!processor_) return;

	release();
}

inline auto idle_task_pusher::push(idle_task task) -> void
{
	if (!processor_) return;

	processor_->push(slot_, task);
}

inline auto idle_task_pusher::push(idle_task_processor::index_t index, idle_task task) -> void
{
	if (!processor_) return;

	processor_->push(slot_, task, index);
}

inline auto idle_task_pusher::release() -> void
{
	processor_->release(slot_);

	processor_ = {};
}

} // clog
