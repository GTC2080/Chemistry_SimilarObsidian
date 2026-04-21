# Reason: This script gives the repo one reusable benchmark-gate runner so Phase 1 verification is a named entrypoint instead of a remembered command chain.

cmake_minimum_required(VERSION 3.21)

function(run_required_benchmark benchmark_name benchmark_path)
  if(NOT DEFINED benchmark_path OR "${benchmark_path}" STREQUAL "")
    message(FATAL_ERROR "Missing benchmark path for ${benchmark_name}.")
  endif()

  message(STATUS "Running ${benchmark_name}: ${benchmark_path}")
  execute_process(
    COMMAND "${benchmark_path}"
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE benchmark_stdout
    ERROR_VARIABLE benchmark_stderr
  )

  string(REPLACE "\r\n" "\n" benchmark_stdout "${benchmark_stdout}")
  string(REPLACE "\r\n" "\n" benchmark_stderr "${benchmark_stderr}")

  if(NOT "${benchmark_stdout}" STREQUAL "")
    message(STATUS "${benchmark_stdout}")
  endif()

  if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR
      "${benchmark_name} failed with exit code ${benchmark_result}.\n"
      "stdout:\n${benchmark_stdout}\n"
      "stderr:\n${benchmark_stderr}")
  endif()
endfunction()

run_required_benchmark("kernel_startup_benchmark" "${KERNEL_STARTUP_BENCH}")
run_required_benchmark("kernel_io_benchmark" "${KERNEL_IO_BENCH}")
run_required_benchmark("kernel_rebuild_benchmark" "${KERNEL_REBUILD_BENCH}")
run_required_benchmark("kernel_query_benchmark" "${KERNEL_QUERY_BENCH}")
