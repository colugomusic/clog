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

struct action_body
{
	std::string name;
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

class undo_redo;

class undo_redo_action
{
public:

	undo_redo_action(std::string name, undo_redo_merge_mode merge_mode) : body_{name, merge_mode} {}

	template <typename Command>
	auto add_do(Command&& command) {
		body_.do_commands.push_back(std::forward<Command>(command));
	}

	template <typename Command>
	auto add_undo(Command&& command) {
		body_.undo_do_commands.push_back(std::forward<Command>(command));
	}

	auto invoke() const -> void {
		body_.invoke();
	}

	auto invoke_undo() const -> void {
		body_.invoke_undo();
	}

	auto commit(undo_redo* mgr) const -> void;

private:

	undo_redo_detail::action_body body_;
};

class undo_redo
{
public:

	auto commit(const undo_redo_action& action) -> void {
		action.commit(this);
	}

	auto commit(undo_redo_detail::action_body action) -> void {
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

	auto commit_merge_all(undo_redo_detail::action_body action) -> void {
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

	auto commit_merge_ends(undo_redo_detail::action_body action) -> void {
		const auto latest_action{get_latest_action()};
		assert (latest_action);
		latest_action->do_commands = std::move(action.do_commands);
	}

	auto commit_no_merging(undo_redo_detail::action_body action) -> void {
		merge_mode_ = action.merge_mode;
		actions_.push_back(std::move(action));
	}

	auto get_latest_action() -> undo_redo_detail::action_body* {
		if (actions_.empty()) return nullptr;
		return &actions_.back();
	}

	auto get_latest_action() const -> const undo_redo_detail::action_body* {
		if (actions_.empty()) return nullptr;
		return &actions_.back();
	}

	auto is_same_action(const undo_redo_detail::action_body& action) const -> bool {
		const auto latest_action{get_latest_action()};
		if (!latest_action) return false;
		return action.name == latest_action->name;
	}

	size_t position_{0};
	std::vector<undo_redo_detail::action_body> actions_;
	undo_redo_merge_mode merge_mode_{undo_redo_merge_mode::none};
};

inline auto undo_redo_action::commit(undo_redo* mgr) const -> void {
	mgr->commit(body_);
}

} // clg
