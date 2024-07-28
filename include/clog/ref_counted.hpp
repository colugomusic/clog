#pragma once

namespace clg {

template <typename RefCounter>
struct ref_counted {
	ref_counted() = default;
	ref_counted(RefCounter counter)
		: counter_{counter}
	{
		counter_.ref();
	}
	~ref_counted() {
		counter_.unref();
	}
	ref_counted(const ref_counted& other) {
		counter_ = other.counter_;
		counter_.ref();
	}
	ref_counted(ref_counted&& other) noexcept {
		counter_ = other.counter_;
		other.counter_ = {};
	}
	ref_counted& operator=(const ref_counted& other) {
		if (this != &other) {
			counter_.unref();
			counter_ = other.counter_;
			counter_.ref();
		}
		return *this;
	}
	ref_counted& operator=(ref_counted&& other) noexcept {
		if (this != &other) {
			counter_.unref();
			counter_ = other.counter_;
			other.counter_ = {};
		}
		return *this;
	}
	[[nodiscard]] auto get_counter() const -> RefCounter { return counter_; }
private:
	RefCounter counter_;
};

} // clg