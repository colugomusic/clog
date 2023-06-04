#pragma once
#include <vector>
namespace clg {
template <typename T>
struct auto_array : private std::vector<T> {
	using std::vector<T>::begin;
	using std::vector<T>::cbegin;
	using std::vector<T>::cend;
	using std::vector<T>::clear;
	using std::vector<T>::crbegin;
	using std::vector<T>::crend;
	using std::vector<T>::end;
	using std::vector<T>::rbegin;
	using std::vector<T>::rend;
	using std::vector<T>::size;
	using size_type = typename std::vector<T>::size_type;
	auto at(size_type pos) -> T& {
		return std::vector<T>::at(pos);
	}
	auto at(size_type pos) const -> const T& {
		return std::vector<T>::at(pos);
	}
	auto operator[](size_type pos) -> T& {
		if (pos >= size()) {
			resize(pos+1);
		}
		return std::vector<T>::operator[](pos);
	}
};
} // clg