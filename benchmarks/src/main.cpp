#include <functional>
#include <random>
#include <benchmark/benchmark.h>
#include "clog/rcv.hpp"
#include "clog/stable_vector.hpp"

static constexpr auto FUNC_COUNT{1000};

// Iterating over an RCV of functions and calling
// them while randomly adding and removing functions
static void BM_rcv(benchmark::State& state) {
    std::random_device dev;
    std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> add(0, 1);
	std::uniform_int_distribution<std::mt19937::result_type> remove(0, 2);
    clg::unsafe_rcv<std::function<void()>> funcs;
    int value{1};
    int i{0};
    auto f = [&value, &i]{
		if (i++ % 100 == 0) {
			value += value;
		}
    };
    for (auto i = 0; i < FUNC_COUNT; i++) {
        funcs.acquire(f);
    }
    for (auto _ : state) {
		if (add(rng) == 0) {
            funcs.acquire(f);
        }
		const auto handles{funcs.active_handles()};
		for (const auto handle : handles) {
            std::invoke(*funcs.get(handle));
            if (remove(rng) == 0) {
                funcs.release(handle);
            }
        }
    }
}
BENCHMARK(BM_rcv);

// Iterating over a stable_vector of functions and calling
// them while randomly adding and removing functions
static void BM_stable_vector(benchmark::State& state) {
    std::random_device dev;
    std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> add(0, 1);
	std::uniform_int_distribution<std::mt19937::result_type> remove(0, 2);
    clg::stable_vector<std::function<void()>> funcs;
    int value{1};
    int i{0};
    auto f = [&value, &i]{
		if (i++ % 100 == 0) {
			value += value;
		}
    };
    for (auto i = 0; i < FUNC_COUNT; i++) {
        funcs.add(f);
    }
    for (auto _ : state) {
		if (add(rng) == 0) {
            funcs.add(f);
        }
        auto beg{funcs.begin()};
        auto end{funcs.end()};
		for (auto pos = beg; pos != end; pos++) {
            std::invoke(*pos);
            if (remove(rng) == 0) {
                funcs.erase(pos);
            }
        }
    }
}
BENCHMARK(BM_stable_vector);

BENCHMARK_MAIN();
