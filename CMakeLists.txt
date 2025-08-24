cmake_minimum_required(VERSION 3.30)
project(clog)
add_library(clog INTERFACE)
add_library(clog::clog ALIAS clog)
target_sources(clog INTERFACE
	FILE_SET HEADERS
	BASE_DIRS
		${CMAKE_CURRENT_LIST_DIR}/include
	FILES
		${CMAKE_CURRENT_LIST_DIR}/include/clog/auto_array.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/box.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/cache.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/data_store.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/expected.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/expire.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/feedback_blocker.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/item_processor.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/pool.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/property.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/rcv.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/ref_counted.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/signal.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/simple_stable_vector.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/small_function.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/stable_vector.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/tree.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/uda.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/undo_redo.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/var.hpp
		${CMAKE_CURRENT_LIST_DIR}/include/clog/vectors.hpp
	)
include(CMakePackageConfigHelpers)
install(TARGETS clog EXPORT clog-targets FILE_SET HEADERS)
install(EXPORT clog-targets FILE clog-targets.cmake NAMESPACE clog:: DESTINATION lib/cmake/clog)
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/clog-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/clog-config.cmake"
    INSTALL_DESTINATION lib/cmake/clog
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/clog-config.cmake" DESTINATION lib/cmake/clog)
