cmake_minimum_required(VERSION 3.0)
project(clog_tests)

include(../cmake/CPM.cmake)

CPMAddPackage("gh:catchorg/Catch2@3.3.2")

add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE
	cxx_std_17
)
target_include_directories(${PROJECT_NAME} PRIVATE
	${catch2_SOURCE_DIR}/src
	../include
)
target_link_libraries(${PROJECT_NAME} PRIVATE
	Catch2::Catch2WithMain
)