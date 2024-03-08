#pragma once

#include <vector>

namespace clg {

template <typename Services, typename Model, typename Action, typename PP>
struct uda {
	uda(Services services)
		: services_(std::move(services))
	{}
	auto model() const -> const Model& {
		return model_;
	}
	auto update() -> void {
		Model old_model = model_;
		Model new_model = model_;
		for (auto action : action_queue_) {
			new_model = apply(std::move(new_model), std::move(action), &pp_);
		}
		action_queue_.clear();
		model_ = std::move(new_model);
		react(services_, std::move(old_model), model_, pp_);
		pp_ = {};
	}
	auto push(Action a) -> void {
		action_queue_.push_back(std::move(a));
	}
private:
	Services services_;
	Model model_;
	PP pp_;
	std::vector<Action> action_queue_;
};

} // clg