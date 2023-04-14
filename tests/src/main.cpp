#include <catch2/catch_test_macros.hpp>
#include <clog/stable_vector.hpp>

TEST_CASE("stable_vector", "[stable_vector]") {
    clg::stable_vector<int> v;

    SECTION("basic add/erase") {
		REQUIRE(v.size() == 0);
		REQUIRE(v.begin() == v.end());
		REQUIRE(v.rbegin() == v.rend());
		auto a = v.add(123);
		REQUIRE(a == 0);
		REQUIRE(v.size() == 1);
		REQUIRE(v.begin() != v.end());
		REQUIRE(v.rbegin() != v.rend());
		REQUIRE(v[a] == 123);
		v[a] = 456;
		REQUIRE(v[a] == 456);
		v.erase(a);
		REQUIRE(v.size() == 0);
		REQUIRE(v.begin() == v.end());
		REQUIRE(v.rbegin() == v.rend());
    }

	SECTION("hole filling") {
		auto a = v.add(12);
		auto b = v.add(34);
		auto c = v.add(56);
		auto d = v.add(78);
		REQUIRE(v.size() == 4);
		REQUIRE(v[a] == 12);
		REQUIRE(v[b] == 34);
		REQUIRE(v[c] == 56);
		REQUIRE(v[d] == 78);
		v.erase(b);
		v.erase(c);
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
}
