cmake_minimum_required(VERSION 3.0)
project(clog_benchmarks)

include(../cmake/CPM.cmake)

set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON)
CPMAddPackage("gh:google/benchmark@1.7.1")

add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE
	cxx_std_17
)
target_include_directories(${PROJECT_NAME} PRIVATE
	${benchmark_SOURCE_DIR}/include
	../include
)
target_link_libraries(${PROJECT_NAME} PRIVATE
	benchmark::benchmark
)