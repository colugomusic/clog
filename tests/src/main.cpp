#include <random>
#include <catch2/catch_test_macros.hpp>
#include <clog/stable_vector.hpp>

TEST_CASE("stable_vector", "[stable_vector]") {
	clg::stable_vector<int> v;

	SECTION("basic add/erase") {
		REQUIRE(v.size() == 0);
		REQUIRE(v.begin() == v.end());
		REQUIRE(v.rbegin() == v.rend());
		auto a = v.add(123);
		REQUIRE(v.is_valid(a));
		REQUIRE(a == 0);
		REQUIRE(v.size() == 1);
		REQUIRE(v.begin() != v.end());
		REQUIRE(v.rbegin() != v.rend());
		REQUIRE(v[a] == 123);
		v[a] = 456;
		REQUIRE(v[a] == 456);
		v.erase(a);
		REQUIRE(!v.is_valid(a));
		REQUIRE(v.size() == 0);
		REQUIRE(v.begin() == v.end());
		REQUIRE(v.rbegin() == v.rend());
	}

	SECTION("hole filling") {
		auto a = v.add(12);
		auto b = v.add(34);
		auto c = v.add(56);
		auto d = v.add(78);
		REQUIRE(v.is_valid(a));
		REQUIRE(v.is_valid(b));
		REQUIRE(v.is_valid(c));
		REQUIRE(v.is_valid(d));
		REQUIRE(v.size() == 4);
		REQUIRE(v[a] == 12);
		REQUIRE(v[b] == 34);
		REQUIRE(v[c] == 56);
		REQUIRE(v[d] == 78);
		v.erase(b);
		v.erase(c);
		REQUIRE(!v.is_valid(b));
		REQUIRE(!v.is_valid(c));
		REQUIRE(v.size() == 2);
		v.add(90);
		std::vector<int> values;
		for (auto value : v) {
			values.push_back(value);
		}
		REQUIRE(values.size() == 3);
		REQUIRE(values[0] == 12);
		REQUIRE(values[1] == 90);
		REQUIRE(values[2] == 78);
		v.add(111);
		v.add(222);
		values.clear();
		for (auto value : v) {
			values.push_back(value);
		}
		REQUIRE(values.size() == 5);
		REQUIRE(values[0] == 12);
		REQUIRE(values[1] == 90);
		REQUIRE(values[2] == 111);
		REQUIRE(values[3] == 78);
		REQUIRE(values[4] == 222);
	}

	SECTION("reverse iteration") {
		v.add(111);
		v.add(222);
		v.add(333);
		v.add(444);
		v.add(555);
		std::vector<int> values;
		for (auto pos = v.rbegin(); pos != v.rend(); pos++) {
			values.push_back(*pos);
		}
		REQUIRE(values.size() == 5);
		REQUIRE(values[0] == 555);
		REQUIRE(values[1] == 444);
		REQUIRE(values[2] == 333);
		REQUIRE(values[3] == 222);
		REQUIRE(values[4] == 111);
	}

	SECTION("erase while iterating forwards") {
		v.add(111);
		v.add(222);
		v.add(333);
		v.add(444);
		v.add(555);
		std::vector<int> values;
		for (auto pos = v.begin(); pos != v.end(); pos++) {
			values.push_back(*pos);
			v.erase(pos);
		}
		REQUIRE(v.size() == 0);
		REQUIRE(values.size() == 5);
		REQUIRE(values[0] == 111);
		REQUIRE(values[1] == 222);
		REQUIRE(values[2] == 333);
		REQUIRE(values[3] == 444);
		REQUIRE(values[4] == 555);
	}

	SECTION("erase while iterating backwards") {
		v.add(111);
		v.add(222);
		v.add(333);
		v.add(444);
		v.add(555);
		std::vector<int> values;
		for (auto pos = v.rbegin(); pos != v.rend(); pos++) {
			values.push_back(*pos);
			v.erase(pos);
		}
		REQUIRE(v.size() == 0);
		REQUIRE(values.size() == 5);
		REQUIRE(values[0] == 555);
		REQUIRE(values[1] == 444);
		REQUIRE(values[2] == 333);
		REQUIRE(values[3] == 222);
		REQUIRE(values[4] == 111);
	}

	SECTION("erasing at the ends") {
		auto a = v.add(111);
		auto b = v.add(222);
		REQUIRE(v.is_valid(a));
		REQUIRE(v.is_valid(b));
		v.add(333);
		v.add(444);
		v.add(555);
		v.erase(a);
		REQUIRE(!v.is_valid(a));
		REQUIRE(v.size() == 4);
		REQUIRE(*v.begin() == 222);
		v.erase(b);
		v.add(111);
		REQUIRE(v.size() == 4);
		REQUIRE(*v.begin() == 111);
	}

	SECTION("random adds and erases") {
		std::random_device dev;
		std::mt19937 rng(dev());
		auto randi = [&rng](int min, int max) {
			std::uniform_int_distribution dist(min, max);
			return dist(rng);
		};
		auto unique_value = [] {
			static int i = 0;
			return i++;
		};
		auto get_handles = [](const clg::stable_vector<int>& v) {
			std::vector<uint32_t> out;
			for (auto pos = v.begin(); pos != v.end(); pos++) {
				out.push_back(pos.index());
			}
			return out;
		};
		auto contains = [](const std::vector<uint32_t>& handles, uint32_t handle) {
			return std::find(handles.begin(), handles.end(), handle) != handles.end();
		};
		auto add_and_check = [&] {
			const auto old_handles = get_handles(v);
			const auto old_size = v.size();
			const auto handle = v.add(unique_value());
			const auto new_handles = get_handles(v);
			const auto new_size = v.size();
			REQUIRE (!contains(old_handles, handle));
			REQUIRE (contains(new_handles, handle));
			REQUIRE (new_size == old_size + 1);
			REQUIRE (new_handles.size() == old_handles.size() + 1);
			for (auto handle : old_handles) {
				REQUIRE(contains(new_handles, handle));
			}
		};
		auto erase_and_check = [&](uint32_t handle) {
			const auto old_handles = get_handles(v);
			const auto old_size = v.size();
			REQUIRE (v.is_valid(handle));
			v.erase(handle);
			REQUIRE (!v.is_valid(handle));
			const auto new_handles = get_handles(v);
			const auto new_size = v.size();
			REQUIRE (contains(old_handles, handle));
			REQUIRE (!contains(new_handles, handle));
			REQUIRE (new_size == old_size - 1);
			REQUIRE (new_handles.size() == old_handles.size() - 1);
			for (auto handle : new_handles) {
				REQUIRE(contains(old_handles, handle));
			}
		};
		clg::stable_vector<int> v;
		for (int i = 0; i < 100; i++) {
			const auto elems_to_add{randi(0, 3)};
			for (int j = 0; j < elems_to_add; j++) {
				add_and_check();
			}
			for (auto pos = v.begin(); pos != v.end(); pos++) {
				if (randi(0, 1) == 0) {
					erase_and_check(pos.index());
				}
			}
		}
	}

	SECTION("clear and insert") {
		auto a = v.add(111);
		auto b = v.add(222);
		auto c = v.add(333);
		REQUIRE (v.is_valid(a));
		REQUIRE (v.is_valid(b));
		REQUIRE (v.is_valid(c));
		v.erase(a);
		v.erase(b);
		v.erase(c);
		REQUIRE (!v.is_valid(a));
		REQUIRE (!v.is_valid(b));
		REQUIRE (!v.is_valid(c));
		v.add(444);
		REQUIRE (v.size() == 1);
		REQUIRE (v.begin() != v.end());
		REQUIRE (v.rbegin() != v.rend());
		REQUIRE (*v.begin() == 444);
		REQUIRE (*v.rbegin() == 444);
	}
}
