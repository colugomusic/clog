#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include "vectors.hpp"

namespace clg {

template <typename T, typename Compare> class tree;
template <typename T, typename Compare> class tree_node;

namespace detail {

template <typename T, typename Compare>
struct tree_node_control_block
{
	using node_type = tree_node<T, Compare>;

	node_type* node;
};

} // detail

template <typename T, typename Compare>
struct tree_node_handle
{
	using control_block_type = detail::tree_node_control_block<T, Compare>;
	using node_type = tree_node<T, Compare>;

	tree_node_handle() = default;

	tree_node_handle(control_block_type* control_block)
		: control_block_{control_block}
	{
	}

	operator bool() const
	{
		return control_block_ != nullptr;
	}

	auto& operator*()
	{
		return *control_block_->node;
	}

	auto& operator*() const
	{
		return *control_block_->node;
	}

	auto operator->()
	{
		return control_block_->node;
	}

	auto operator->() const
	{
		return control_block_->node;
	}

	auto operator<(const tree_node_handle<T, Compare>& rhs) const -> bool
	{
		return control_block_ < rhs.control_block_;
	}

private:

	auto get_node() -> node_type&
	{
		return *control_block_->node;
	}

	auto get_node() const -> const node_type&
	{
		return *control_block_->node;
	}

	control_block_type* control_block_{};
	friend class tree_node<T, Compare>;
};

template <typename T, typename Compare>
class tree_node
{
public:

	using control_block_type = detail::tree_node_control_block<T, Compare>;
	using node_handle_type = tree_node_handle<T, Compare>;
	using node_type = tree_node<T, Compare>;
	using compare_type = Compare;

	auto get_depth() const { return depth_; }
	auto get_parent() const { return parent_; }
	auto& get_value() { return value_; }
	auto& get_value() const { return value_; }
	auto& get_children() const { return children_; }

	operator const T&() const
	{
		return value_;
	}

	template <typename U>
	auto set_value(U&& value)
	{
		auto& parent { parent_.get_node() };
		const auto pos { vectors::sorted::find(parent.children_, value_, parent.compare_) };

		auto node { std::move(*pos) };

		parent.children_.erase(pos);

		node.value_ = std::forward<U>(value);

		vectors::sorted::unique::checked::insert(&parent.children_, std::move(node), parent.compare_);
	}

	template <typename U>
	auto find_or_add(U&& value)
	{
		node_type node(make_handle(), value, depth_ + 1, compare_);

		const auto begin{std::cbegin(children_)};
		const auto end{std::cend(children_)};

		assert (std::is_sorted(begin, end, compare_));

		auto pos{std::lower_bound(begin, end, node, compare_)};

		if (pos == end || compare_(value, *pos))
		{
			pos = children_.insert(pos, std::move(node));

			return std::make_pair(pos->make_handle(), true);
		}

		return std::make_pair(pos->make_handle(), false);
	}

	template <typename U>
	auto find(U&& value) -> node_handle_type
	{
		node_type node(make_handle(), std::forward<U>(value), depth_ + 1, compare_);

		const auto pos = vectors::sorted::find(children_, std::move(node), compare_);

		if (pos == std::cend(children_))
		{
			return node_handle_type{};
		}

		return pos->make_handle();
	}

	template <typename U>
	auto add(U&& value) -> node_handle_type
	{
		node_type node(make_handle(), std::forward<U>(value), depth_ + 1, compare_);

		const auto pos = vectors::sorted::unique::overwrite(&children_, std::move(node), compare_);

		return pos->make_handle();
	}

	template <typename U, typename... Tail>
	auto add(U&& head, Tail&&... tail) -> node_handle_type
	{
		node_type* node;

		auto handle { find(head) };

		if (!handle)
		{
			node = &add(std::forward<U>(head)).get_node();
		}
		else
		{
			node = &handle.get_node();
		}

		return node->add(std::forward<Tail>(tail)...);
	}

	//auto add_nodes(const std::vector<T>& path) -> node_handle_type
	//{
	//	node_type* node{this};

	//	for (const auto& part : path)
	//	{
	//		auto handle{node->find(part)};

	//		if (!handle)
	//		{
	//			node = &add(part).get_node();
	//		}
	//		else
	//		{
	//			node = &handle.get_node();
	//		}
	//	}

	//	return node->make_handle();
	//}

	auto remove(node_handle_type child) -> void
	{
		vectors::sorted::unique::checked::erase(&children_, child.get_node(), compare_);
	}

	template <typename U>
	auto remove(U&& value) -> void
	{
		node_type node(make_handle(), std::forward<U>(value), depth_ + 1, compare_);

		vectors::sorted::unique::checked::erase(&children_, std::move(node), compare_);
	}

	template <typename Visitor>
	auto visit_breadth_first(Visitor&& visitor) const -> node_handle_type
	{
		std::deque<const node_type*> queue;

		queue.push_back(this);

		while (!queue.empty())
		{
			const auto node { queue.front() };
			queue.pop_front();

			const auto handle{node->make_handle()};

			if (visitor(handle))
			{
				return handle;
			}

			for (const auto& child : node->children_)
			{
				queue.push_back(&child);
			}
		}

		return {};
	}

	template <typename Visitor>
	auto visit_depth_first(Visitor&& visitor) const -> node_handle_type
	{
		const auto handle{make_handle()};

		if (visitor(handle))
		{
			return handle;
		}

		for (const auto& child : children_)
		{
			const auto result{child.visit_depth_first(visitor)};

			if (result)
			{
				return result;
			}
		}

		return {};
	}

	template <typename U>
	auto find(U&& value) const -> node_handle_type
	{
		const auto pos { vectors::sorted::find(children_, value, compare_) };

		if (pos == std::cend(children_))
		{
			return node_handle_type{};
		}

		const auto& node { *pos };

		return node.make_handle();
	}

	auto make_handle() const -> node_handle_type
	{
		return node_handle_type{control_block_.get()};
	}

	tree_node(node_type&& rhs) noexcept
		: parent_{rhs.parent_}
		, value_{std::move(rhs.value_)}
		, depth_{rhs.depth_}
		, compare_{rhs.compare_}
		, children_{std::move(rhs.children_)}
		, control_block_{std::move(rhs.control_block_)}
	{
		control_block_->node = this;
	}
	
	auto operator=(node_type&& rhs) noexcept -> node_type&
	{
		parent_ = rhs.parent_;
		value_ = std::move(rhs.value_);
		depth_ = rhs.depth_;
		compare_ = rhs.compare_;
		children_ = std::move(rhs.children_);
		control_block_ = std::move(rhs.control_block_);
		control_block_->node = this;

		return *this;
	}

	tree_node(const node_type& rhs)
		: parent_{rhs.parent_}
		, value_{rhs.value_}
		, depth_{rhs.depth_}
		, compare_{rhs.compare_}
		, children_{rhs.children_}
		, control_block_{std::make_unique<control_block_type>()}
	{
		control_block_->node = this;
	}

	auto operator=(const node_type& rhs) -> node_type&
	{
		parent_ = rhs.parent_;
		value_ = rhs.value_;
		depth_ = rhs.depth_;
		compare_ = rhs.compare_;
		children_ = rhs.children_;
		control_block_ = std::make_unique<control_block_type>();
		control_block_->node = this;

		return *this;
	}

private:

	tree_node(node_handle_type parent, T initial_value, int depth, Compare compare = Compare{})
		: parent_{parent}
		, value_{initial_value}
		, depth_{depth}
		, compare_{compare}
		, control_block_{std::make_unique<control_block_type>()}
	{
		control_block_->node = this;
	}

	node_handle_type parent_{};
	T value_;
	int depth_;
	Compare compare_;
	std::vector<node_type> children_;
	std::unique_ptr<control_block_type> control_block_;

	friend class tree<T, Compare>;
};

template <typename T, typename Compare = std::less<T>>
class tree
{
public:

	using node_type = tree_node<T, Compare>;
	using node_handle_type = tree_node_handle<T, Compare>;

	tree(T root_value, const Compare& compare = Compare{})
		: root_{node_handle_type{}, root_value, 0, compare}
		, compare_{compare}
	{
	}

	auto get_root() const
	{
		return root_.make_handle();
	}

	template <typename U>
	auto add(U&& value) -> node_handle_type
	{
		return root_.add(std::forward<U>(value));
	}

	template <typename... Path>
	auto add(Path&&... path) -> node_handle_type
	{
		return root_.add(std::forward<Path>(path)...);
	}

	template <typename U>
	auto find(U&& value) -> node_handle_type
	{
		return root_.find(std::forward<U>(value));
	}

	//auto add_nodes(const std::vector<T>& path) -> node_handle_type
	//{
	//	return root_.add_nodes(path);
	//}

	auto remove(node_handle_type node) -> void
	{
		root_.remove(node);
	}

	template <typename Visitor>
	auto search_breadth_first(Visitor&& visitor) const -> node_handle_type
	{
		return root_.visit_breadth_first(std::forward<Visitor>(visitor));
	}

	template <typename Visitor>
	auto search_depth_first(Visitor&& visitor) const -> node_handle_type
	{
		return root_.visit_depth_first(std::forward<Visitor>(visitor));
	}

private:

	node_type root_;
	Compare compare_;
};

} // clg