# Reason: This file creates the first executable test target and registers it with CTest.

add_executable(
  kernel_api_tests
  src/api/kernel_api_tests.cpp
  src/api/kernel_api_tests_attachment_legacy_contract.cpp
  src/api/kernel_api_tests_core_contract.cpp
  src/api/kernel_api_tests_attachment_diagnostics.cpp
  src/api/kernel_api_tests_attachment_lifecycle.cpp
  src/api/kernel_api_tests_attachment_lifecycle_recovery.cpp
  src/api/kernel_api_tests_attachment_lifecycle_surface.cpp
  src/api/kernel_api_tests_attachment_lifecycle_watcher.cpp
  src/api/kernel_api_tests_attachment_public_surface.cpp
  src/api/kernel_api_tests_rebuild_runtime.cpp
  src/api/kernel_api_tests_runtime_diagnostics.cpp
  src/api/kernel_api_tests_runtime_diagnostics_core.cpp
  src/api/kernel_api_tests_runtime_diagnostics_watcher.cpp
  src/api/kernel_api_tests_runtime_recovery.cpp
  src/api/kernel_api_tests_runtime_recovery_interruptions.cpp
  src/api/kernel_api_tests_search_expanded.cpp
  src/api/kernel_api_tests_search_legacy.cpp
  src/api/kernel_api_tests_search_public_surface.cpp
  src/api/kernel_api_tests_watcher_smoke.cpp
  src/api/kernel_api_test_support.cpp
  src/support/test_support.cpp
)
target_link_libraries(kernel_api_tests PRIVATE chem_kernel)
target_include_directories(kernel_api_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal ${PROJECT_SOURCE_DIR}/tests/include)
add_test(NAME kernel_api_tests COMMAND kernel_api_tests)

add_executable(parser_tests src/parser/parser_tests.cpp src/support/test_support.cpp)
target_link_libraries(parser_tests PRIVATE chem_kernel)
target_include_directories(parser_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal ${PROJECT_SOURCE_DIR}/tests/include)
add_test(NAME parser_tests COMMAND parser_tests)

add_executable(search_tests src/search/search_tests.cpp src/support/test_support.cpp)
target_link_libraries(search_tests PRIVATE chem_kernel)
target_include_directories(search_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal ${PROJECT_SOURCE_DIR}/tests/include)
add_test(NAME search_tests COMMAND search_tests)

add_executable(watcher_tests src/watcher/watcher_tests.cpp src/support/test_support.cpp)
target_link_libraries(watcher_tests PRIVATE chem_kernel)
target_include_directories(watcher_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal ${PROJECT_SOURCE_DIR}/tests/include)
add_test(NAME watcher_tests COMMAND watcher_tests)

