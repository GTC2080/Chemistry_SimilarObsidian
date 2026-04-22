# Reason: This file creates the first benchmark executables without introducing external benchmark frameworks.

add_executable(kernel_startup_benchmark startup/startup_benchmark.cpp)
target_link_libraries(kernel_startup_benchmark PRIVATE chem_kernel)
target_include_directories(kernel_startup_benchmark PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal)

add_executable(kernel_io_benchmark io/io_benchmark.cpp)
target_link_libraries(kernel_io_benchmark PRIVATE chem_kernel)
target_include_directories(kernel_io_benchmark PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal)

add_executable(kernel_rebuild_benchmark
  rebuild/rebuild_benchmark.cpp
  rebuild/rebuild_benchmark_chemistry.cpp)
target_link_libraries(kernel_rebuild_benchmark PRIVATE chem_kernel)
target_include_directories(kernel_rebuild_benchmark PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal)

add_executable(kernel_query_benchmark
  query/query_benchmark.cpp
  query/query_benchmark_chemistry.cpp
  query/query_benchmark_domain.cpp)
target_link_libraries(kernel_query_benchmark PRIVATE chem_kernel)
target_include_directories(kernel_query_benchmark PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal)
