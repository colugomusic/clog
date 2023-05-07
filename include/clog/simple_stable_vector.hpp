#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace clg {

namespace simple_stable_vector_detail {

template <typename T, size_t N>
struct alignas(alignof(T)) aligned_array : public std::array<T, N> {};

template <typename T>
struct cell_t {
	using storage_t = aligned_array<std::byte, sizeof(T)>;
	cell_t() = default;
	cell_t(const cell_t<T>& rhs)
		: occupied_{rhs.occupied_}
	{
		initialize_value(rhs.get_value());
	}
	cell_t(cell_t<T>&& rhs) noexcept 
		: occupied_{rhs.occupied_}
	{
		initialize_value(std::move(rhs.get_value()));
		rhs.destroy_value();
	}
	~cell_t() {
		destroy_value();
	}
	template <typename... Args>
	auto initialize_value(Args&&... args) -> void {
		::new(std::addressof(storage_)) T{std::forward<Args>(args)...};
		occupied_ = true;
	}
	auto is_occupied() const -> bool {
		return occupied_;
	}
	auto get_value() -> T& {
		const auto ptr{reinterpret_cast<T*>(std::addressof(storage_))};
		return *ptr;
	}
	auto get_value() const -> const T& {
		const auto ptr{reinterpret_cast<const T*>(std::addressof(storage_))};
		return *ptr;
	}
	auto destroy_value() -> void {
		get_value().~T();
		occupied_ = false;
	}
private:
	bool occupied_{false};
	storage_t storage_;
};

template <typename T>
using cell_vector_t = std::vector<cell_t<T>>;

} // simple_stable_vector_detail

template <class T>
class simple_stable_vector
{
public:
	template <typename... Args>
	auto add(Args&&... args) -> uint32_t {
		if (size_t(position_) == cells_.size()) {
			return push_back(std::forward<Args>(args)...);
		}
		return insert(std::forward<Args>(args)...);
	}
	auto erase(uint32_t index) -> void {
		auto& cell{cells_[index]};
		cell.destroy_value();
		if (static_cast<int32_t>(index) < position_) {
			position_ = static_cast<int32_t>(index);
		}
		size_--;
	}
	auto at(uint32_t index) -> T& {
		return cells_.at(index).get_value();
	}
	auto at(uint32_t index) const -> const T& {
		return cells_.at(index).get_value();
	}
	auto is_valid(uint32_t index) const -> bool {
		return size_ > 0 && index < size_ && cells_[index].is_occupied();
	}
	auto operator[](uint32_t index) -> T& {
		return cells_[index].get_value();
	}
	auto operator[](uint32_t index) const -> const T& {
		return cells_[index].get_value();
	}
	auto size() const { return size_; }
private:
	template <typename... Args>
	auto push_back(Args&&... args) -> uint32_t {
		const auto handle{static_cast<uint32_t>(position_)};
		cells_.resize(position_ + 1);
		auto& cell{cells_[position_]};
		cell.initialize_value(std::forward<Args>(args)...);
		position_++;
		size_++;
		return handle;
	}
	template <typename... Args>
	auto insert(Args&&... args) -> uint32_t {
		const auto handle{static_cast<uint32_t>(position_)};
		cells_[position_].initialize_value(std::forward<Args>(args)...);
		position_ = find_next_empty_cell(position_);
		size_++;
		return handle;
	}
	auto find_next_empty_cell(int32_t position) -> int32_t {
		for (;;) {
			position++;
			if (position == size_) {
				return position;
			}
			if (!cells_[position].is_occupied()) {
				return position;
			}
		}
	}
	int32_t position_{0};
	size_t size_{0};
	simple_stable_vector_detail::cell_vector_t<T> cells_;
};

} // clg