# Reason: This file defines the smallest correct root build graph using parallel top-level directories.

cmake_minimum_required(VERSION 3.21)
project(chem_kernel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KERNEL_BUILD_TESTS "Build kernel tests" ON)
option(KERNEL_BUILD_BENCHMARKS "Build kernel benchmarks" ON)

include(cmake/project-options.cmake)

add_subdirectory(src)

if(KERNEL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(KERNEL_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(KERNEL_BUILD_TESTS AND KERNEL_BUILD_BENCHMARKS)
  add_custom_target(
    kernel_benchmark_gates
    COMMAND
      ${CMAKE_COMMAND}
      -DKERNEL_STARTUP_BENCH=$<TARGET_FILE:kernel_startup_benchmark>
      -DKERNEL_IO_BENCH=$<TARGET_FILE:kernel_io_benchmark>
      -DKERNEL_REBUILD_BENCH=$<TARGET_FILE:kernel_rebuild_benchmark>
      -DKERNEL_QUERY_BENCH=$<TARGET_FILE:kernel_query_benchmark>
      -P ${PROJECT_SOURCE_DIR}/cmake/run_benchmark_gates.cmake
    DEPENDS
      kernel_startup_benchmark
      kernel_io_benchmark
      kernel_rebuild_benchmark
      kernel_query_benchmark
    USES_TERMINAL
    COMMENT "Run all kernel benchmark regression gates"
  )

  add_custom_target(
    kernel_phase_gate
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${PROJECT_BINARY_DIR} -C $<CONFIG> --output-on-failure
    COMMAND
      ${CMAKE_COMMAND}
      -DKERNEL_STARTUP_BENCH=$<TARGET_FILE:kernel_startup_benchmark>
      -DKERNEL_IO_BENCH=$<TARGET_FILE:kernel_io_benchmark>
      -DKERNEL_REBUILD_BENCH=$<TARGET_FILE:kernel_rebuild_benchmark>
      -DKERNEL_QUERY_BENCH=$<TARGET_FILE:kernel_query_benchmark>
      -P ${PROJECT_SOURCE_DIR}/cmake/run_benchmark_gates.cmake
    DEPENDS
      kernel_api_tests
      parser_tests
      search_tests
      watcher_tests
      kernel_startup_benchmark
      kernel_io_benchmark
      kernel_rebuild_benchmark
      kernel_query_benchmark
    USES_TERMINAL
    COMMENT "Run the full Phase 1 gate: tests plus benchmark regression gates"
  )
endif()
