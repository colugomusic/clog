#pragma once

#include <cassert>
#include <functional>
#include <string>
#include <deque>
#include <vector>

namespace clg {

using undo_redo_command = std::function<void()>;
enum class undo_redo_merge_mode { none, ends, all };

namespace undo_redo_detail {

template <typename KeyType>
struct action_body
{
	KeyType key;
	undo_redo_merge_mode merge_mode;
	std::deque<undo_redo_command> do_commands;
	std::deque<undo_redo_command> undo_commands;

	auto invoke() const -> void
	{
		for (const auto& command : do_commands) {
			std::invoke(command);
		}
	}

	auto invoke_undo() const -> void
	{
		for (const auto& command : undo_commands) {
			std::invoke(command);
		}
	}
};

} // undo_redo_detail

template <typename KeyType>
class undo_redo;

template <typename KeyType>
class undo_redo_action
{
public:
	using body_type = undo_redo_detail::action_body<KeyType>;

	undo_redo_action(KeyType key, undo_redo_merge_mode merge_mode = undo_redo_merge_mode::none) : body_{key, merge_mode} {}

	template <typename Command>
	auto add_do(Command&& command) {
		body_.do_commands.push_back(std::forward<Command>(command));
	}

	template <typename Command>
	auto add_undo(Command&& command) {
		body_.undo_commands.push_back(std::forward<Command>(command));
	}

	auto invoke() const -> void {
		body_.invoke();
	}

	auto invoke_undo() const -> void {
		body_.invoke_undo();
	}

	auto commit(undo_redo<KeyType>* mgr) const -> void;

private:

	body_type body_;
};

template <typename KeyType>
class undo_redo
{
public:
	using action_type = undo_redo_action<KeyType>;
	using action_body_type = undo_redo_detail::action_body<KeyType>;

	auto commit(action_body_type action) -> void {
		if (merge_mode_ == undo_redo_merge_mode::all) {
			if (!is_same_action(action)) {
				merge_mode_ = undo_redo_merge_mode::none;
				commit_no_merging(std::move(action));
				return;
			}

			commit_merge_all(std::move(action));
			return;
		}

		if (merge_mode_ == undo_redo_merge_mode::ends) {
			if (!is_same_action(action)) {
				merge_mode_ = undo_redo_merge_mode::none;
				commit_no_merging(std::move(action));
				return;
			}
			
			commit_merge_ends(std::move(action));
			return;
		}

		commit_no_merging(std::move(action));
	}

	//template <typename Action,
	//	typename action = std::remove_cv_t<std::remove_reference_t<Action>>,
	//	typename e = std::enable_if_t<std::is_same_v<action, action_type>>>
	template <typename Action>
	auto commit(Action&& action) -> void {
		action.commit(this);
	}

	template <typename Action>
	auto invoke_and_commit(Action&& action) -> void {
		action.invoke();
		action.commit(this);
	}

	auto undo() -> bool {
		if (position_ == 0) {
			return false;
		}
		
		position_--;
		actions_[position_].invoke_undo();
	}

	auto redo() -> bool {
		if (position_ == actions_.size()) {
			return false;
		}

		actions_[position_].invoke();
		position_++;
	}

private:

	auto commit_merge_all(action_body_type action) -> void {
		const auto latest_action{get_latest_action()};
		assert (latest_action);
		latest_action->undo_commands.insert(
			latest_action->undo_commands.begin(),
			action.undo_commands.begin(),
			action.undo_commands.end());
		latest_action->do_commands.insert(
			latest_action->do_commands.end(),
			action.do_commands.begin(),
			action.do_commands.end());
	}

	auto commit_merge_ends(action_body_type action) -> void {
		const auto latest_action{get_latest_action()};
		assert (latest_action);
		latest_action->do_commands = std::move(action.do_commands);
	}

	auto commit_no_merging(action_body_type action) -> void {
		merge_mode_ = action.merge_mode;
		actions_.push_back(std::move(action));
	}

	auto get_latest_action() -> action_body_type* {
		if (actions_.empty()) return nullptr;
		return &actions_.back();
	}

	auto get_latest_action() const -> const action_body_type* {
		if (actions_.empty()) return nullptr;
		return &actions_.back();
	}

	auto is_same_action(const action_body_type& action) const -> bool {
		const auto latest_action{get_latest_action()};
		if (!latest_action) return false;
		return action.key == latest_action->key;
	}

	size_t position_{0};
	std::vector<undo_redo_detail::action_body<KeyType>> actions_;
	undo_redo_merge_mode merge_mode_{undo_redo_merge_mode::none};
};

template <typename KeyType>
inline auto undo_redo_action<KeyType>::commit(undo_redo<KeyType>* mgr) const -> void {
	mgr->commit(body_);
}

} // clg
