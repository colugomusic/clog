#pragma once
#include <vector>
namespace clg {
template <typename T>
struct auto_array : private std::vector<T> {
	using size_type = typename std::vector<T>::size_type;
	auto operator[](size_type pos) -> T& {
		if (pos >= size()) {
			resize(pos+1);
		}
		return std::vector<T>::operator[](pos);
	}
};
} // clg