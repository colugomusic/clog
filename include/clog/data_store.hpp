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

template <typename KeyType, typename... Types>
class data_store {
public:

	auto add() -> KeyType {
		const auto key{key_++};
		const auto index{(std::get<data_vector<Types>>(vectors_).push_back(), ...)};
		book_.set(key, index);
		return key;
	}

	auto add(Types&&... values) -> KeyType {
		const auto key{key_++};
		const auto index{(std::get<data_vector<Types>>(vectors_).push_back(std::forward<Types>(values)), ...)};
		book_.set(key, index);
		return key;
	}

	auto erase(KeyType key) -> void {
		const auto index{book_.get_index(key)};
		const auto new_size{(std::get<data_vector<Types>>(vectors_).erase(index), ...)};
		if (new_size > 0 && index < new_size) {
			// The new vector size is the index of the element
			// that was moved into the erased element's place.
			const auto moved_key{book_.get_key(new_size)};
			book_.set(moved_key, index);
		}
		book_.erase(key, new_size);
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
	auto get(KeyType key) -> T& {
		return std::get<data_vector<T>>(vectors_)[book_.get_index(key)];
	}

	template <typename T>
	auto get(KeyType key) const -> const T& {
		return std::get<data_vector<T>>(vectors_)[book_.get_index(key)];
	}

private:

	using vectors = std::tuple<data_vector<Types>...>;

	struct bimap {
		auto erase(KeyType key, size_t index) -> void {
			key_to_index_.erase(key);
			index_to_key_.erase(index);
		}

		auto get_index(KeyType key) const -> size_t {
			const auto pos{key_to_index_.find(key)};
			assert (pos != key_to_index_.end());
			return pos->second;
		}

		auto get_key(size_t index) const -> KeyType {
			const auto pos{index_to_key_.find(index)};
			assert (pos != index_to_key_.end());
			return pos->second;
		}

		auto set(KeyType key, size_t index) -> void {
			key_to_index_[key] = index;
			index_to_key_[index] = key;
		}

	private:

		std::unordered_map<KeyType, size_t> key_to_index_;
		std::unordered_map<size_t, KeyType> index_to_key_;
	};

	KeyType key_{0};
	vectors vectors_;
	bimap book_;
};

} // clg