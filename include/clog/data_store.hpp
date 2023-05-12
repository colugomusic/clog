#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace clg {

class invalid_handle : public std::logic_error {
public:
    explicit invalid_handle(const std::string& what) : std::logic_error(what) {}
    explicit invalid_handle(const char* what) : std::logic_error(what) {}
};
class invalid_index : public std::logic_error {
public:
    explicit invalid_index(const std::string& what) : std::logic_error(what) {}
    explicit invalid_index(const char* what) : std::logic_error(what) {}
};

template <typename T>
class data_vector {
public:

	// Returns: The index of the newly added element.
	auto push_back() -> size_t {
		data_.push_back(T{});
		return size() - 1;
	}

	// Returns: The index of the newly added element.
	template <typename U>
	auto push_back(U&& value) -> size_t {
		data_.push_back(std::forward<U>(value));
		return size() - 1;
	}

	// Returns: The index of the newly added element.
	template <typename... Args>
	auto emplace_back(Args&&... args) -> size_t {
		data_.emplace_back(std::forward<Args>(args)...);
		return size() - 1;
	}

	// Returns: The new size of the vector after the
	// element was erased.
	auto erase(size_t index) -> size_t {
		assert (size() > index);
		// Move the back element into the space we are
		// creating, if it's not already at the back
		if (size() > 1 && index < size() - 1) {
			data_[index] = std::move(data_.back());
		}
		data_.resize(size() - 1);
		return size();
	}

	auto at(size_t index) -> T& { return data_.at(index); }
	auto at(size_t index) const -> const T& { return data_.at(index); }
	auto operator[](size_t index) -> T& { return at(index); }
	auto operator[](size_t index) const -> const T& { return at(index); }
	auto size() const { return data_.size(); }
	auto begin() { return data_.begin(); }
	auto begin() const { return data_.begin(); }
	auto rbegin() { return data_.rbegin(); }
	auto rbegin() const { return data_.rbegin(); }
	auto cbegin() const { return data_.cbegin(); }
	auto crbegin() const { return data_.crbegin(); }
	auto end() { return data_.end(); }
	auto end() const { return data_.end(); }
	auto rend() { return data_.rend(); }
	auto rend() const { return data_.rend(); }
	auto cend() const { return data_.cend(); }
	auto crend() const { return data_.crend(); }

private:

	std::vector<T> data_;
};

struct data_handle {
	uint64_t value{0};
	data_handle() = default;
	constexpr data_handle(uint64_t value) : value{value} {}
	operator bool() const { return value > 0; }
	operator uint64_t() const { return value; }
	auto operator++() -> data_handle& { value++; return *this; }
	auto operator++(int) -> data_handle { data_handle old{*this}; operator++(); return old; }
	auto operator==(const data_handle& rhs) const { return value == rhs.value; }
	auto operator<(const data_handle& rhs) const { return value < rhs.value; }
};

struct data_index {
	size_t value{0};
	data_index() = default;
	constexpr data_index(size_t value) : value{value} {}
	operator size_t() const { return value; }
	auto operator<(const data_index& rhs) const { return value < rhs.value; }
};

struct data_handle_hash {
	auto operator()(const data_handle& handle) const -> size_t {
		return std::hash<int64_t>()(handle.value);
	}
};

struct data_index_hash {
	auto operator()(const data_index& index) const -> size_t {
		return std::hash<size_t>()(index.value);
	}
};

template <typename... Types>
class data_store {
public:

	auto add() -> data_handle {
		const auto handle{handle_++};
		const auto index{(std::get<data_vector<Types>>(vectors_).push_back(), ...)};
		book_.set(handle, index);
		return handle;
	}

	auto add(Types&&... values) -> data_handle {
		const auto handle{handle_++};
		const auto index{(std::get<data_vector<Types>>(vectors_).push_back(std::forward<Types>(values)), ...)};
		book_.set(handle, index);
		return handle;
	}

	auto erase(data_handle handle) -> void {
		const auto index{get_index(handle)};
		const auto new_size{(std::get<data_vector<Types>>(vectors_).erase(index), ...)};
		if (new_size > 0 && index < clg::data_index{new_size}) {
			// The new vector size is the index of the element
			// that was moved into the erased element's place.
			const auto moved_handle{book_.get_handle(new_size)};
			book_.set(moved_handle, index);
		}
		book_.erase(handle, new_size);
	}

	// Returns the current index for the specified handle.
	// The returned index will be invalidated by later
	// calls to erase().
	auto get_index(data_handle handle) const -> data_index {
		return book_.get_index(handle);
	}

	auto get_id(data_index index) const -> data_handle {
		return book_.get_handle(index);
	}

	template <typename T> auto get() -> data_vector<T>& { return std::get<data_vector<T>>(vectors_); }
	template <typename T> auto get() const -> const data_vector<T>& { return std::get<data_vector<T>>(vectors_); }
	template <typename T> auto get(data_handle handle) -> T& {
		try { return get<T>(get_index(handle)); }
		catch (const std::out_of_range& err) { throw invalid_handle{err.what()}; }
	}
	template <typename T> auto get(data_handle handle) const -> const T& {
		try { return get<T>(get_index(handle)); }
		catch (const std::out_of_range& err) { throw invalid_handle{err.what()}; }
	}
	template <typename T> auto get(data_index index) -> T& {
		try { return std::get<data_vector<T>>(vectors_)[index]; }
		catch (const std::out_of_range& err) { throw invalid_index{err.what()}; }
	}
	template <typename T> auto get(data_index index) const -> const T& {
		try { return std::get<data_vector<T>>(vectors_)[index]; }
		catch (const std::out_of_range& err) { throw invalid_index{err.what()}; }
	}

private:

	using vectors = std::tuple<data_vector<Types>...>;

	struct bimap {
		auto erase(data_handle handle, data_index index) -> void {
			handle_to_index_.erase(handle);
			index_to_handle_.erase(index);
		}

		auto get_index(data_handle handle) const -> data_index {
			const auto pos{handle_to_index_.find(handle)};
			assert (pos != handle_to_index_.end());
			return pos->second;
		}

		auto get_handle(data_index index) const -> data_handle {
			const auto pos{index_to_handle_.find(index)};
			assert (pos != index_to_handle_.end());
			return pos->second;
		}

		auto set(data_handle handle, data_index index) -> void {
			handle_to_index_[handle] = index;
			index_to_handle_[index] = handle;
		}

	private:

		std::unordered_map<data_handle, data_index, data_handle_hash> handle_to_index_;
		std::unordered_map<data_index, data_handle, data_index_hash> index_to_handle_;
	};

	data_handle handle_{1};
	vectors vectors_;
	bimap book_;
};

} // clg