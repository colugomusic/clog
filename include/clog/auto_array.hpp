#pragma once
#include <vector>
namespace clg {
template <typename T>
struct auto_array : public std::vector<T> {
	auto operator[](typename std::vector<T>::size_type pos) -> T& {
		if (pos >= size()) {
			std::vector<T>::resize(pos+1);
		}
		return std::vector<T>::operator[](pos);
	}
private:
	using std::vector<T>::emplace_back;
	using std::vector<T>::erase;
	using std::vector<T>::insert;
	using std::vector<T>::pop_back;
	using std::vector<T>::push_back;
	using std::vector<T>::resize;
};
} // clg