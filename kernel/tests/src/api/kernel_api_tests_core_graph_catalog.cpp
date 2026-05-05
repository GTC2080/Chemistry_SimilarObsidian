// Reason: This file verifies the host-facing graph catalog backed by the C++ kernel index.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void write_graph_note(kernel_handle* handle, const char* rel_path, const std::string& content) {
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const kernel_status status = kernel_write_note(
      handle,
      rel_path,
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition);
  require_true(status.code == KERNEL_OK, "graph setup note write should succeed");
  require_true(disposition == KERNEL_WRITE_WRITTEN, "graph setup note should be written");
}

bool graph_has_node(const kernel_graph& graph, const std::string& id, const bool ghost) {
  for (size_t index = 0; index < graph.node_count; ++index) {
    if (graph.nodes[index].id != nullptr &&
        std::string(graph.nodes[index].id) == id &&
        graph.nodes[index].ghost == (ghost ? 1 : 0)) {
      return true;
    }
  }
  return false;
}

std::string find_ghost_id_by_name(const kernel_graph& graph, const std::string& name) {
  for (size_t index = 0; index < graph.node_count; ++index) {
    if (graph.nodes[index].ghost != 0 &&
        graph.nodes[index].name != nullptr &&
        std::string(graph.nodes[index].name) == name) {
      return graph.nodes[index].id == nullptr ? std::string{} : graph.nodes[index].id;
    }
  }
  return {};
}

bool graph_has_link(
    const kernel_graph& graph,
    const std::string& source,
    const std::string& target,
    const std::string& kind) {
  for (size_t index = 0; index < graph.link_count; ++index) {
    if (graph.links[index].source != nullptr &&
        graph.links[index].target != nullptr &&
        graph.links[index].kind != nullptr &&
        std::string(graph.links[index].source) == source &&
        std::string(graph.links[index].target) == target &&
        std::string(graph.links[index].kind) == kind) {
      return true;
    }
  }
  return false;
}

void test_query_graph_returns_live_notes_and_kernel_edges() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_graph_note(handle, "source.md", "# Source\n[[Target]]\n[[Missing Target]]\n#shared\n");
  write_graph_note(handle, "target.md", "# Target\n");
  write_graph_note(handle, "other.md", "# Other\n#shared\n");
  write_graph_note(handle, "folder/a.md", "# Folder A\n");
  write_graph_note(handle, "folder/b.md", "# Folder B\n");

  kernel_graph graph{};
  expect_ok(kernel_query_graph(handle, 64, &graph));
  require_true(graph_has_node(graph, "source.md", false), "graph should include source live node");
  require_true(graph_has_node(graph, "target.md", false), "graph should include target live node");
  require_true(graph_has_node(graph, "folder/a.md", false), "graph should include nested live node");

  const std::string ghost_id = find_ghost_id_by_name(graph, "Missing Target");
  require_true(!ghost_id.empty(), "graph should include unresolved wikilink ghost node");
  require_true(
      graph_has_link(graph, "source.md", "target.md", "link"),
      "graph should expose resolved wikilink edge");
  require_true(
      graph_has_link(graph, "source.md", ghost_id, "link"),
      "graph should expose unresolved wikilink edge to ghost node");
  require_true(
      graph_has_link(graph, "source.md", "other.md", "tag") ||
          graph_has_link(graph, "other.md", "source.md", "tag"),
      "graph should expose tag co-occurrence edge");
  require_true(
      graph_has_link(graph, "folder/a.md", "folder/b.md", "folder") ||
          graph_has_link(graph, "folder/b.md", "folder/a.md", "folder"),
      "graph should expose same-folder edge");
  kernel_free_graph(&graph);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_graph_limit_and_argument_validation() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_graph_note(handle, "a.md", "# A\n");
  write_graph_note(handle, "b.md", "# B\n");

  kernel_graph graph{};
  expect_ok(kernel_query_graph(handle, 1, &graph));
  require_true(graph.node_count == 1, "graph note limit should cap live node count");
  kernel_free_graph(&graph);

  require_true(
      kernel_query_graph(nullptr, 1, &graph).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "graph query should require handle");
  require_true(
      kernel_query_graph(handle, 0, &graph).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "graph query should reject zero limit");
  require_true(
      kernel_query_graph(handle, 1, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "graph query should require output pointer");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_enriched_graph_json_is_kernel_owned() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_graph_note(handle, "alpha.md", "# Alpha\n[[Beta]]\n");
  write_graph_note(handle, "beta.md", "# Beta\n");

  kernel_owned_buffer buffer{};
  expect_ok(kernel_query_enriched_graph_json(handle, 64, &buffer));
  const std::string json(buffer.data == nullptr ? "" : buffer.data, buffer.size);
  kernel_free_buffer(&buffer);

  require_true(
      json.find("\"neighbors\":{\"alpha.md\":[\"beta.md\"],\"beta.md\":[\"alpha.md\"]}") !=
          std::string::npos,
      "enriched graph JSON should expose kernel-owned neighbor adjacency");
  require_true(
      json.find("\"linkPairs\":[\"alpha.md->beta.md\",\"beta.md->alpha.md\"]") !=
          std::string::npos,
      "enriched graph JSON should expose kernel-owned directed link pairs");
  require_true(
      kernel_query_enriched_graph_json(nullptr, 1, &buffer).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "enriched graph query should require handle");
  require_true(
      kernel_query_enriched_graph_json(handle, 0, &buffer).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "enriched graph query should reject zero limit");
  require_true(
      kernel_query_enriched_graph_json(handle, 1, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "enriched graph query should require output pointer");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_graph_default_limits_are_kernel_owned() {
  size_t graph_limit = 0;
  expect_ok(kernel_get_graph_default_limit(&graph_limit));
  require_true(graph_limit == 2048, "graph default limit should be kernel-owned");
  require_true(
      kernel_get_graph_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "graph default limit should require output pointer");

  size_t backlink_limit = 0;
  expect_ok(kernel_get_backlink_default_limit(&backlink_limit));
  require_true(backlink_limit == 64, "backlink default limit should be kernel-owned");
  require_true(
      kernel_get_backlink_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "backlink default limit should require output pointer");
}

}  // namespace

void run_kernel_api_core_graph_contract_tests() {
  test_query_graph_returns_live_notes_and_kernel_edges();
  test_query_graph_limit_and_argument_validation();
  test_query_enriched_graph_json_is_kernel_owned();
  test_graph_default_limits_are_kernel_owned();
}
