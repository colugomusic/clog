#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace clg {

template <class T>
class stable_vector;

namespace stable_vector_detail {

struct info_t {
	int32_t prev{-1};
	int32_t next{-1};
};

template <typename T, size_t N>
struct alignas(alignof(T)) aligned_array : public std::array<T, N> {};

template <typename T>
struct cell_t {
	using storage_t = aligned_array<std::byte, sizeof(T)>;
	cell_t() = default;
	cell_t(const cell_t<T>& rhs)
		: info_{rhs.info_}
	{
		initialize_value(rhs.get_value());
	}
	cell_t(cell_t<T>&& rhs) noexcept 
		: info_{std::move(rhs.info_)}
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
	}
	auto get_info() -> info_t& {
		return info_;
	}
	auto get_info() const -> const info_t& {
		return info_;
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
	}
private:
	info_t info_;
	storage_t storage_;
};

template <typename T>
using cell_vector_t = std::vector<cell_t<T>>;

template <typename T, bool Const>
struct iterator_base_t
{
	using iterator_category = std::forward_iterator_tag;
	using difference_type   = std::ptrdiff_t;
	using value_type        = std::conditional_t<Const, std::add_const_t<T>, T>;
	using pointer           = std::add_pointer_t<value_type>;
	using reference         = std::add_lvalue_reference_t<value_type>;
	using vector_type       = std::conditional_t<Const, const cell_vector_t<value_type>, cell_vector_t<value_type>>;
	iterator_base_t(vector_type* cells, int32_t position)
		: cells_{cells}
		, position_{position}
	{}
	auto operator*() const -> reference {
		return (*cells_)[position_].get_value();
	}
	auto operator->() -> pointer {
		return &(*cells_)[position_].get_value();
	}
	auto index() const {
		return position_;
	}
	friend bool operator== (const iterator_base_t<T, Const>& a, const iterator_base_t<T, Const>& b) {
		return a.position_ == b.position_;
	}
	friend bool operator!= (const iterator_base_t<T, Const>& a, const iterator_base_t<T, Const>& b) {
		return a.position_ != b.position_;
	}
protected:
	vector_type* cells_;
	int32_t position_;
};

template <typename T, bool Const>
struct iterator_t : public iterator_base_t<T, Const>
{
	using iterator_base_t<T, Const>::cells_;
	using iterator_base_t<T, Const>::position_;
	using iterator_base_t<T, Const>::vector_type;
	iterator_t(vector_type* cells, int32_t position)
		: iterator_base_t<T, Const>{cells, position}
	{}
	auto operator++() -> iterator_t& {
		position_ = (*cells_)[position_].get_info().next;
		return *this;
	}
	auto operator++(int) -> iterator_t {
		iterator_t tmp = *this;
		++(*this);
		return tmp;
	}
};

template <typename T, bool Const>
struct reverse_iterator_t : public iterator_base_t<T, Const>
{
	using iterator_base_t<T, Const>::cells_;
	using iterator_base_t<T, Const>::position_;
	using iterator_base_t<T, Const>::vector_type;
	reverse_iterator_t(cell_vector_t<T>* cells, int32_t position)
		: iterator_base_t<T, Const>{cells, position}
	{}
	auto operator++() -> reverse_iterator_t& {
		position_ = (*cells_)[position_].get_info().prev;
		return *this;
	}
	auto operator++(int) -> reverse_iterator_t {
		reverse_iterator_t tmp = *this;
		++(*this);
		return tmp;
	}
};

} // stable_vector_detail

template <class T>
class stable_vector
{
public:
	using iterator_t = stable_vector_detail::iterator_t<T, false>;
	using reverse_iterator_t = stable_vector_detail::reverse_iterator_t<T, false>;
	using const_iterator_t = stable_vector_detail::iterator_t<T, true>;
	using const_reverse_iterator_t = stable_vector_detail::reverse_iterator_t<T, true>;
	template <typename... Args>
	auto add(Args&&... args) -> uint32_t {
		if (size_t(position_) == cells_.size()) {
			return push_back(std::forward<Args>(args)...);
		}
		return insert(std::forward<Args>(args)...);
	}
	auto erase(iterator_t pos) -> void { erase(pos.index()); }
	auto erase(const_iterator_t pos) -> void { erase(pos.index()); }
	auto erase(reverse_iterator_t pos) -> void { erase(pos.index()); }
	auto erase(const_reverse_iterator_t pos) -> void { erase(pos.index()); }
	auto erase(uint32_t index) -> void {
		auto& cell{cells_[index]};
		auto& info{cell.get_info()};
		cell.destroy_value();
		if (info.prev >= 0) {
			auto& prev_info{cells_[info.prev].get_info()};
			prev_info.next = info.next;
		} else {
			front_ = info.next;
		}
		if (info.next >= 0) {
			auto& next_info{cells_[info.next].get_info()};
			next_info.prev = info.prev;
		} else {
			back_ = info.prev;
		}
		if (static_cast<int32_t>(index) < position_) {
			position_ = static_cast<int32_t>(index);
		}
		size_--;
	}
	auto operator[](uint32_t index) -> T& {
		return cells_[index].get_value();
	}
	auto operator[](uint32_t index) const -> const T& {
		return cells_[index].get_value();
	}
	auto size() const { return size_; }
	auto begin() { return iterator_t(&cells_, front_); }
	auto begin() const { return const_iterator_t(&cells_, front_); }
	auto end() { return iterator_t(&cells_, -1); }
	auto end() const { return const_iterator_t(&cells_, -1); }
	auto rbegin() { return reverse_iterator_t(&cells_, back_); }
	auto rbegin() const { return const_reverse_iterator_t(&cells_, back_); }
	auto rend() { return reverse_iterator_t(&cells_, -1); }
	auto rend() const { return const_reverse_iterator_t(&cells_, -1); }
	auto cbegin() const { return const_iterator_t(&cells_, front_); }
	auto cend() const { return const_iterator_t(&cells_, -1); }
	auto crbegin() const { return const_reverse_iterator_t(&cells_, back_); }
	auto crend() const { return const_reverse_iterator_t(&cells_, -1); }
private:
	template <typename... Args>
	auto push_back(Args&&... args) -> uint32_t {
		const auto handle{static_cast<uint32_t>(position_)};
		cells_.resize(position_ + 1);
		auto& cell{cells_[position_]};
		cell.initialize_value(std::forward<Args>(args)...);
		auto& info{cell.get_info()};
		info.prev = position_-1;
		if (info.prev >= 0) {
			auto& prev_info{cells_[info.prev].get_info()};
			prev_info.next = position_;
		} else {
			front_ = position_;
		}
		back_ = position_;
		position_++;
		size_++;
		return handle;
	}
	template <typename... Args>
	auto insert(Args&&... args) -> uint32_t {
		const auto handle{static_cast<uint32_t>(position_)};
		cells_[position_].initialize_value(std::forward<Args>(args)...);
		auto& info{cells_[position_].get_info()};
		info.prev = position_-1;
		if (front_ > position_) {
			info.prev = -1;
			info.next = front_;
			auto& front_info{cells_[front_].get_info()};
			front_info.prev = position_;
			front_ = position_;
		}
		else {
			if (info.prev >= 0) {
				auto& prev_info{cells_[info.prev].get_info()};
				info.next = prev_info.next;
				prev_info.next = position_;
				if (info.next >= 0) {
					auto& next_info{cells_[info.next].get_info()};
					next_info.prev = position_;
				}
			} else {
				// Must be the only element
				info.next = -1;
			}
		}
		if (position_ > back_) {
			back_ = position_;
		}
		position_ = find_next_empty_cell(position_);
		size_++;
		return handle;
	}
	auto find_next_empty_cell(int32_t position) -> int32_t {
		for (;;) {
			auto& info{cells_[position].get_info()};
			position++;
			if (info.next != position) {
				return position;
			}
		}
	}
	int32_t front_{-1};
	int32_t back_{-1};
	int32_t position_{0};
	size_t size_{0};
	stable_vector_detail::cell_vector_t<T> cells_;
};

} // clg