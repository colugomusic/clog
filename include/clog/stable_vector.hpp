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

template <typename T> struct dummy { T value; info_t info; };

template <typename T, size_t N>
struct alignas(alignof(dummy<T>)) aligned_array : public std::array<T, N> {};

template <typename T>
struct cell_t {
	using storage_t = aligned_array<std::byte, sizeof(info_t) + sizeof(T)>;
	cell_t() {
		initialize_info();
	}
	cell_t(const cell_t<T>& rhs) = delete;
	cell_t(cell_t<T>&& rhs) noexcept {
		initialize_info(rhs.get_info());
		initialize_value(std::move(rhs.get_value()));
		rhs.destroy_info();
		rhs.destroy_value();
	}
	template <typename... Args>
	auto initialize_value(Args&&... args) -> void {
		::new(std::addressof(storage_) + sizeof(info_t)) T{std::forward<Args>(args)...};
	}
	auto get_info() -> info_t& {
		const auto ptr{reinterpret_cast<info_t*>(std::addressof(storage_))};
		return *ptr;
	}
	auto get_value() -> T& {
		const auto ptr{reinterpret_cast<T*>(std::addressof(storage_) + sizeof(info_t))};
		return *ptr;
	}
	auto destroy_value() -> void {
		get_value().~T();
	}
private:
	auto initialize_info(info_t info = {}) -> void {
		::new(std::addressof(storage_)) stable_vector_detail::info_t{info};
	}
	auto destroy_info() -> void {
		get_info().~info_t();
	}
	storage_t storage_;
};

template <typename T>
using cell_vector_t = std::vector<cell_t<T>>;

template <typename T>
struct iterator_base_t
{
	using iterator_category = std::forward_iterator_tag;
	using difference_type   = std::ptrdiff_t;
	using value_type        = T;
	using pointer           = T*;
	using reference         = T&;
	iterator_base_t(cell_vector_t<T>* cells, int32_t position)
		: cells_{cells}
		, position_{position}
	{}
	auto operator*() const -> reference {
		return (*cells_)[position_].get_value();
	}
	auto operator->() -> pointer {
		return &(*cells_)[position_].get_value();
	}
protected:
	cell_vector_t<T>* cells_;
	int32_t position_;
	friend class stable_vector<T>;
};

template <typename T>
struct iterator_t : iterator_base_t<T>
{
	iterator_t(cell_vector_t<T>* cells, int32_t position)
		: iterator_base_t<T>{cells, position}
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
	friend bool operator== (const iterator_t& a, const iterator_t& b) { return a.position_ == b.position_; };
	friend bool operator!= (const iterator_t& a, const iterator_t& b) { return a.position_ != b.position_; };
};

template <typename T>
struct reverse_iterator_t : iterator_base_t<T>
{
	reverse_iterator_t(cell_vector_t<T>* cells, int32_t position)
		: iterator_base_t<T>{cells, position}
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
	friend bool operator== (const reverse_iterator_t& a, const reverse_iterator_t& b) { return a.position_ == b.position_; };
	friend bool operator!= (const reverse_iterator_t& a, const reverse_iterator_t& b) { return a.position_ != b.position_; };
};

} // stable_vector_detail

template <class T>
class stable_vector
{
public:
	using iterator_t = stable_vector_detail::iterator_t<T>;
	using reverse_iterator_t = stable_vector_detail::reverse_iterator_t<T>;
	template <typename... Args>
	auto add(Args&&... args) -> int32_t {
		if (size_t(position_) == cells_.size()) {
			return push_back(std::forward<Args>(args)...);
		}
		return insert(std::forward<Args>(args)...);
	}
	auto erase(iterator_t pos) -> void {
		erase(pos.position_);
	}
	auto erase(int32_t index) -> void {
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
		if (index < position_) {
			position_ = index;
		}
	}
	auto operator[](int32_t index) -> T& {
		return cells_[index].get_value();
	}
    auto begin() { return iterator_t(&cells_, front_); }
    auto end() { return iterator_t(&cells_, -1); }
    auto rbegin() { return reverse_iterator_t(&cells_, back_); }
    auto rend() { return reverse_iterator_t(&cells_, -1); }
private:
	template <typename... Args>
	auto push_back(Args&&... args) -> int32_t {
		const auto handle{position_};
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
		return handle;
	}
	template <typename... Args>
	auto insert(Args&&... args) -> int32_t {
		const auto handle{position_};
		cells_[position_].initialize_value(std::forward<Args>(args)...);
		auto& info{cells_[position_].get_info()};
		info.prev = position_-1;
		if (info.prev >= 0) {
			auto& prev_info{cells_[info.prev].get_info()};
			info.next = prev_info.next;
			prev_info.next = position_;
			if (info.next >= 0) {
				auto& next_info{cells_[info.next].get_info()};
				next_info.prev = position_;
			}
		} else {
			front_ = position_;
		}
		if (position_ > back_) {
			back_ = position_;
		}
		position_ = find_next_empty_cell(position_);
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
	stable_vector_detail::cell_vector_t<T> cells_;
};

} // clg