# Reason: This file creates the first executable test target and registers it with CTest.

add_executable(
  kernel_api_tests
  src/api/kernel_api_tests.cpp
  src/api/kernel_api_tests_attachment_legacy_contract.cpp
  src/api/kernel_api_tests_core_contract.cpp
  src/api/kernel_api_tests_attachment_diagnostics.cpp
  src/api/kernel_api_attachment_diagnostics_helpers.cpp
  src/api/kernel_api_tests_attachment_diagnostics_recount.cpp
  src/api/kernel_api_tests_attachment_diagnostics_snapshot.cpp
  src/api/kernel_api_tests_attachment_lifecycle.cpp
  src/api/kernel_api_tests_attachment_lifecycle_recovery.cpp
  src/api/kernel_api_tests_attachment_lifecycle_recovery_reopen.cpp
  src/api/kernel_api_tests_attachment_lifecycle_recovery_startup.cpp
  src/api/kernel_api_tests_attachment_lifecycle_surface.cpp
  src/api/kernel_api_tests_attachment_lifecycle_watcher.cpp
  src/api/kernel_api_tests_attachment_lifecycle_watcher_backoff.cpp
  src/api/kernel_api_tests_attachment_lifecycle_watcher_rescan.cpp
  src/api/kernel_api_tests_attachment_public_surface.cpp
  src/api/kernel_api_tests_attachment_public_surface_catalog.cpp
  src/api/kernel_api_tests_attachment_public_surface_refs.cpp
  src/api/kernel_api_tests_pdf_surface.cpp
  src/api/kernel_api_tests_pdf_surface_metadata.cpp
  src/api/kernel_api_tests_pdf_surface_anchor.cpp
  src/api/kernel_api_tests_pdf_surface_reference.cpp
  src/api/kernel_api_tests_pdf_surface_reference_note_refs.cpp
  src/api/kernel_api_tests_pdf_surface_reference_referrers.cpp
  src/api/kernel_api_tests_pdf_diagnostics.cpp
  src/api/kernel_api_tests_pdf_diagnostics_snapshot.cpp
  src/api/kernel_api_tests_pdf_diagnostics_recount.cpp
  src/api/kernel_api_tests_rebuild_runtime.cpp
  src/api/kernel_api_tests_rebuild_runtime_background.cpp
  src/api/kernel_api_tests_rebuild_runtime_background_lifecycle.cpp
  src/api/kernel_api_tests_rebuild_runtime_background_wait.cpp
  src/api/kernel_api_tests_rebuild_runtime_core.cpp
  src/api/kernel_api_tests_rebuild_runtime_status.cpp
  src/api/kernel_api_tests_core_contract_state.cpp
  src/api/kernel_api_tests_core_contract_write.cpp
  src/api/kernel_api_tests_core_contract_parser.cpp
  src/api/kernel_api_tests_runtime_diagnostics.cpp
  src/api/kernel_api_tests_runtime_diagnostics_core.cpp
  src/api/kernel_api_tests_runtime_diagnostics_events_recovery.cpp
  src/api/kernel_api_tests_runtime_diagnostics_state.cpp
  src/api/kernel_api_tests_runtime_diagnostics_rebuild.cpp
  src/api/kernel_api_tests_runtime_diagnostics_watcher.cpp
  src/api/kernel_api_tests_runtime_recovery.cpp
  src/api/kernel_api_tests_runtime_recovery_disk_truth.cpp
  src/api/kernel_api_tests_runtime_recovery_disk_truth_catchup.cpp
  src/api/kernel_api_tests_runtime_recovery_disk_truth_replace.cpp
  src/api/kernel_api_tests_runtime_recovery_interruptions.cpp
  src/api/kernel_api_tests_runtime_recovery_interruptions_apply.cpp
  src/api/kernel_api_tests_runtime_recovery_interruptions_backoff.cpp
  src/api/kernel_api_tests_runtime_recovery_startup.cpp
  src/api/kernel_api_tests_runtime_recovery_startup_cleanup.cpp
  src/api/kernel_api_tests_runtime_recovery_startup_tail.cpp
  src/api/kernel_api_tests_search_expanded.cpp
  src/api/kernel_api_tests_search_expanded_pagination.cpp
  src/api/kernel_api_tests_search_expanded_filters.cpp
  src/api/kernel_api_tests_search_expanded_filters_surface.cpp
  src/api/kernel_api_tests_search_expanded_filters_validation.cpp
  src/api/kernel_api_tests_search_expanded_ranking.cpp
  src/api/kernel_api_tests_search_expanded_snippet.cpp
  src/api/kernel_api_tests_search_expanded_snippet_pagination.cpp
  src/api/kernel_api_tests_search_legacy.cpp
  src/api/kernel_api_tests_search_legacy_graph.cpp
  src/api/kernel_api_tests_search_legacy_note.cpp
  src/api/kernel_api_tests_search_legacy_note_basic.cpp
  src/api/kernel_api_tests_search_legacy_note_surface.cpp
  src/api/kernel_api_tests_search_public_surface.cpp
  src/api/kernel_api_tests_watcher_smoke.cpp
  src/api/kernel_api_tests_watcher_smoke_basic.cpp
  src/api/kernel_api_tests_watcher_smoke_close_edges.cpp
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

add_executable(
  search_tests
  src/search/search_tests.cpp
  src/search/search_test_support.cpp
  src/search/search_tests_note_core.cpp
  src/search/search_tests_note_core_persistence.cpp
  src/search/search_tests_note_core_query.cpp
  src/search/search_tests_snippet_pagination.cpp
  src/search/search_tests_snippet.cpp
  src/search/search_tests_page_contract.cpp
  src/search/search_tests_ranking.cpp
  src/search/search_tests_incremental_refresh.cpp
  src/support/test_support.cpp
)
target_link_libraries(search_tests PRIVATE chem_kernel)
target_include_directories(search_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal ${PROJECT_SOURCE_DIR}/tests/include)
add_test(NAME search_tests COMMAND search_tests)

add_executable(
  watcher_tests
  src/watcher/watcher_tests.cpp
  src/watcher/watcher_test_support.cpp
  src/watcher/watcher_tests_event_coalescing.cpp
  src/watcher/watcher_tests_session.cpp
  src/watcher/watcher_tests_refresh_integration.cpp
  src/watcher/watcher_tests_continuity.cpp
  src/support/test_support.cpp
)
target_link_libraries(watcher_tests PRIVATE chem_kernel)
target_include_directories(watcher_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/internal ${PROJECT_SOURCE_DIR}/tests/include)
add_test(NAME watcher_tests COMMAND watcher_tests)

