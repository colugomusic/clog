#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

namespace clg {

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

	auto operator[](size_t index) -> T& { return data_[index]; }
	auto operator[](size_t index) const -> const T& { return data_[index]; }
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

template <typename... Types>
class data_store {
public:

	struct handle_t {
		uint64_t value{0};
		auto operator++() -> handle_t& { value++; return *this; }
		auto operator++(int) -> handle_t { handle_t old{*this}; operator++(); return old; }
	};

	auto add() -> handle_t {
		const auto handle{handle_++};
		const auto index{(std::get<data_vector<Types>>(vectors_).push_back(), ...)};
		book_.set(handle, index);
		return handle;
	}

	auto add(Types&&... values) -> handle_t {
		const auto handle{handle_++};
		const auto index{(std::get<data_vector<Types>>(vectors_).push_back(std::forward<Types>(values)), ...)};
		book_.set(handle, index);
		return handle;
	}

	auto erase(handle_t handle) -> void {
		const auto index{get_index(handle)};
		const auto new_size{(std::get<data_vector<Types>>(vectors_).erase(index), ...)};
		if (new_size > 0 && index < new_size) {
			// The new vector size is the index of the element
			// that was moved into the erased element's place.
			const auto moved_handle{book_.get_handle(new_size)};
			book_.set(moved_handle, index);
		}
		book_.erase(handle, new_size);
	}

	template <typename T>
	auto get() -> data_vector<T>& {
		return std::get<data_vector<T>>(vectors_);
	}

	template <typename T>
	auto get() const -> const data_vector<T>& {
		return std::get<data_vector<T>>(vectors_);
	}

	template <typename T>
	auto get(handle_t handle) -> T& {
		return std::get<data_vector<T>>(vectors_)[get_index(handle)];
	}

	template <typename T>
	auto get(handle_t handle) const -> const T& {
		return std::get<data_vector<T>>(vectors_)[get_index(handle)];
	}

	// Returns the current index for the specified handle.
	// The returned index will be invalidated by later
	// calls to erase().
	auto get_index(handle_t handle) const -> size_t {
		return book_.get_index(handle);
	}

private:

	using vectors = std::tuple<data_vector<Types>...>;

	struct bimap {
		auto erase(handle_t handle, size_t index) -> void {
			handle_to_index_.erase(handle);
			index_to_handle_.erase(index);
		}

		auto get_index(handle_t handle) const -> size_t {
			const auto pos{handle_to_index_.find(handle)};
			assert (pos != handle_to_index_.end());
			return pos->second;
		}

		auto get_handle(size_t index) const -> handle_t {
			const auto pos{index_to_handle_.find(index)};
			assert (pos != index_to_handle_.end());
			return pos->second;
		}

		auto set(handle_t handle, size_t index) -> void {
			handle_to_index_[handle] = index;
			index_to_handle_[index] = handle;
		}

	private:

		std::unordered_map<handle_t, size_t> handle_to_index_;
		std::unordered_map<size_t, handle_t> index_to_handle_;
	};

	handle_t handle_{0};
	vectors vectors_;
	bimap book_;
};

} // clg