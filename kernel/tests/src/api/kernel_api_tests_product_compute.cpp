// Reason: Cover product compute rules at the kernel ABI boundary so Tauri
// command code can remain a thin bridge.

#include "kernel/c_api.h"

#include "support/test_support.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

std::string nullable_string(const char* value) {
  return value == nullptr ? std::string() : std::string(value);
}

std::string buffer_to_string(const kernel_owned_buffer& buffer) {
  if (buffer.data == nullptr || buffer.size == 0) {
    return {};
  }
  return std::string(buffer.data, buffer.size);
}

template <std::size_t N>
std::string utf8_string(const char8_t (&value)[N]) {
  return std::string(reinterpret_cast<const char*>(value), N - 1);
}

void test_truth_diff_text_delta_routes_by_extension() {
  const std::string prev = "a";
  const std::string curr(260, 'x');
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(
          prev.data(),
          prev.size(),
          curr.data(),
          curr.size(),
          "csv",
          &result),
      "truth diff text delta");

  require_true(result.count == 1, "text delta should return one award");
  require_true(nullable_string(result.awards[0].attr) == "science", "csv delta routes to science");
  require_true(result.awards[0].amount == 5, "text delta should preserve legacy floor formula");
  require_true(
      result.awards[0].reason == KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA,
      "text delta should return structured reason");
  require_true(result.awards[0].detail == nullptr, "text delta should not include detail");

  kernel_free_truth_diff_result(&result);
  require_true(result.awards == nullptr && result.count == 0, "truth diff free should reset result");
}

void test_truth_diff_code_language_award() {
  const std::string prev = "note";
  const std::string curr =
      "note\n"
      "```rust\n"
      "fn main() {}\n"
      "```";
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(
          prev.data(),
          prev.size(),
          curr.data(),
          curr.size(),
          "md",
          &result),
      "truth diff code language");

  require_true(result.count == 1, "new rust code fence should return one award");
  require_true(nullable_string(result.awards[0].attr) == "engineering", "rust routes to engineering");
  require_true(result.awards[0].amount == 8, "code language award amount should match legacy rule");
  require_true(
      result.awards[0].reason == KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE,
      "code award should return structured reason");
  require_true(nullable_string(result.awards[0].detail) == "rust", "code award should include language detail");

  kernel_free_truth_diff_result(&result);
}

void test_truth_diff_molecular_line_award() {
  const std::string prev = "atom-a";
  const std::string curr =
      "atom-a\n"
      "atom-b\n"
      "atom-c";
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(
          prev.data(),
          prev.size(),
          curr.data(),
          curr.size(),
          "mol",
          &result),
      "truth diff molecular lines");

  require_true(result.count == 1, "molecular line growth should return one award");
  require_true(nullable_string(result.awards[0].attr) == "creation", "mol line growth routes to creation");
  require_true(result.awards[0].amount == 10, "molecular line award should scale by added lines");
  require_true(
      result.awards[0].reason == KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT,
      "molecular award should return structured reason");
  require_true(result.awards[0].detail == nullptr, "molecular award should not include detail");

  kernel_free_truth_diff_result(&result);
}

void test_truth_diff_empty_and_invalid_args() {
  const std::string curr = "content";
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(nullptr, 0, curr.data(), curr.size(), "md", &result),
      "truth diff empty previous content");
  require_true(result.count == 0, "empty previous content should return no awards");

  require_true(
      kernel_compute_truth_diff(curr.data(), curr.size(), curr.data(), curr.size(), "md", nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "truth diff should reject null result");
  require_true(
      kernel_compute_truth_diff(nullptr, 1, curr.data(), curr.size(), "md", &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth diff should reject null non-empty previous buffer");
  require_true(
      kernel_compute_truth_diff(curr.data(), curr.size(), curr.data(), curr.size(), nullptr, &result)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "truth diff should reject null extension");
}

void test_truth_award_reason_keys_are_kernel_owned() {
  const char* key = nullptr;

  require_ok_status(
      kernel_get_truth_award_reason_key(KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA, &key),
      "truth reason text delta key");
  require_true(nullable_string(key) == "textDelta", "text delta reason key should come from kernel");

  require_ok_status(
      kernel_get_truth_award_reason_key(KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE, &key),
      "truth reason code language key");
  require_true(nullable_string(key) == "codeLanguage", "code language reason key should come from kernel");

  require_ok_status(
      kernel_get_truth_award_reason_key(KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT, &key),
      "truth reason molecular edit key");
  require_true(nullable_string(key) == "molecularEdit", "molecular edit reason key should come from kernel");

  require_true(
      kernel_get_truth_award_reason_key(KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth reason key should reject null output");
  require_true(
      kernel_get_truth_award_reason_key(static_cast<kernel_truth_award_reason>(0), &key).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth reason key should reject unknown enum values");
}

void test_file_extension_derivation_is_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string csv_path = "Folder.v1/Spectrum.CSV";
  require_ok_status(
      kernel_derive_file_extension_from_path(csv_path.data(), csv_path.size(), &buffer),
      "derive extension from POSIX path");
  require_true(
      buffer_to_string(buffer) == "csv",
      "file extension derivation should lower-case the final path segment extension");
  kernel_free_buffer(&buffer);

  const std::string xyz_path = "C:\\vault\\Mol.XYZ";
  require_ok_status(
      kernel_derive_file_extension_from_path(xyz_path.data(), xyz_path.size(), &buffer),
      "derive extension from Windows path");
  require_true(
      buffer_to_string(buffer) == "xyz",
      "file extension derivation should handle Windows separators");
  kernel_free_buffer(&buffer);

  const std::string folder_dot_path = "Folder.With.Dot/README";
  require_ok_status(
      kernel_derive_file_extension_from_path(
          folder_dot_path.data(),
          folder_dot_path.size(),
          &buffer),
      "derive extension from extensionless path");
  require_true(
      buffer_to_string(buffer).empty(),
      "file extension derivation should ignore dots in parent directories");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_derive_file_extension_from_path(nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "file extension derivation should reject null non-empty path");
  require_true(
      kernel_derive_file_extension_from_path(csv_path.data(), csv_path.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "file extension derivation should reject null output");
}

void test_database_column_type_normalization_is_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string allowed_type = "number";
  require_ok_status(
      kernel_normalize_database_column_type(allowed_type.data(), allowed_type.size(), &buffer),
      "normalize allowed database column type");
  require_true(
      buffer_to_string(buffer) == "number",
      "database column type normalization should preserve allowed types");
  kernel_free_buffer(&buffer);

  const std::string invalid_type = "formula";
  require_ok_status(
      kernel_normalize_database_column_type(invalid_type.data(), invalid_type.size(), &buffer),
      "normalize invalid database column type");
  require_true(
      buffer_to_string(buffer) == "text",
      "database column type normalization should fall back to text");
  kernel_free_buffer(&buffer);

  require_ok_status(
      kernel_normalize_database_column_type(nullptr, 0, &buffer),
      "normalize empty database column type");
  require_true(
      buffer_to_string(buffer) == "text",
      "empty database column type should fall back to text");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_normalize_database_column_type(nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "database column type normalization should reject null non-empty input");
  require_true(
      kernel_normalize_database_column_type(allowed_type.data(), allowed_type.size(), nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "database column type normalization should reject null output");
}

void test_database_payload_normalization_json_is_kernel_owned() {
  kernel_owned_buffer buffer{};
  const std::string input =
      "{\"columns\":["
      "{\"id\":\" a \",\"name\":\" Amount \",\"type\":\"number\"},"
      "{\"id\":\"b\",\"name\":\"   \",\"type\":\"formula\"},"
      "{\"id\":\"c\",\"name\":\"Missing\"}"
      "],\"rows\":["
      "{\"id\":\" row1 \",\"cells\":{\"a\":1,\"extra\":true}},"
      "{\"id\":\"row2\",\"cells\":{\"b\":[\"x\"]}},"
      "42"
      "]}";

  require_ok_status(
      kernel_normalize_database_json(input.data(), input.size(), &buffer),
      "normalize database JSON payload");

  const std::string expected =
      "{\"columns\":["
      "{\"id\":\"a\",\"name\":\"Amount\",\"type\":\"number\"},"
      "{\"id\":\"b\",\"name\":\"Untitled\",\"type\":\"text\"},"
      "{\"id\":\"c\",\"name\":\"Missing\",\"type\":\"text\"}"
      "],\"rows\":["
      "{\"id\":\"row1\",\"cells\":{\"a\":1,\"b\":\"\",\"c\":\"\"}},"
      "{\"id\":\"row2\",\"cells\":{\"a\":\"\",\"b\":[\"x\"],\"c\":\"\"}}"
      "]}";
  require_true(
      buffer_to_string(buffer) == expected,
      "database JSON normalization should trim columns/rows, normalize types, and fill missing cells");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_normalize_database_json(nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "database JSON normalization should reject null non-empty input");
  require_true(
      kernel_normalize_database_json(input.data(), input.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "database JSON normalization should reject null output");
  const std::string invalid = "{\"columns\":[";
  require_true(
      kernel_normalize_database_json(invalid.data(), invalid.size(), &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "database JSON normalization should reject invalid JSON");
}

void test_paper_compile_plan_json_is_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string workspace = "E:\\tmp\\nexus-paper-1";
  const std::string templ = " Standard-Thesis ";
  const std::string image_a = "C:\\vault\\figs\\a.png";
  const std::string image_b = "C:\\vault\\figs\\b.png";
  const std::string image_c = "C:\\vault\\other\\c.png";
  const std::string image_loose = "loose.png";
  const char* image_paths[] = {
      image_a.data(),
      image_b.data(),
      image_c.data(),
      image_loose.data()};
  const size_t image_path_sizes[] = {
      image_a.size(),
      image_b.size(),
      image_c.size(),
      image_loose.size()};
  const std::string blank_csl = "  ";
  const std::string bib = " refs.bib ";
  const std::string separator = ";";

  require_ok_status(
      kernel_build_paper_compile_plan_json(
          workspace.data(),
          workspace.size(),
          templ.data(),
          templ.size(),
          image_paths,
          image_path_sizes,
          4,
          blank_csl.data(),
          blank_csl.size(),
          bib.data(),
          bib.size(),
          separator.data(),
          separator.size(),
          &buffer),
      "paper compile plan JSON");

  const std::string expected =
      "{\"templateArgs\":[\"-V\",\"documentclass=report\",\"-V\",\"fontsize=12pt\","
      "\"-V\",\"geometry:margin=1in\"],\"cslPath\":\"\",\"bibliographyPath\":\"refs.bib\","
      "\"resourcePath\":\"E:\\\\tmp\\\\nexus-paper-1;C:\\\\vault\\\\figs;C:\\\\vault\\\\other\"}";
  require_true(
      buffer_to_string(buffer) == expected,
      "paper compile plan should trim template/csl/bib and dedupe image parent resource roots");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_build_paper_compile_plan_json(
          workspace.data(),
          workspace.size(),
          templ.data(),
          templ.size(),
          image_paths,
          nullptr,
          1,
          nullptr,
          0,
          nullptr,
          0,
          separator.data(),
          separator.size(),
          &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "paper compile plan should reject missing image path sizes");
  require_true(
      kernel_build_paper_compile_plan_json(
          workspace.data(),
          workspace.size(),
          templ.data(),
          templ.size(),
          nullptr,
          image_path_sizes,
          1,
          nullptr,
          0,
          nullptr,
          0,
          separator.data(),
          separator.size(),
          &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "paper compile plan should reject missing image path pointers");
  require_true(
      kernel_build_paper_compile_plan_json(
          workspace.data(),
          workspace.size(),
          templ.data(),
          templ.size(),
          image_paths,
          image_path_sizes,
          1,
          nullptr,
          1,
          nullptr,
          0,
          separator.data(),
          separator.size(),
          &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "paper compile plan should reject null non-empty csl path");
  require_true(
      kernel_build_paper_compile_plan_json(
          workspace.data(),
          workspace.size(),
          templ.data(),
          templ.size(),
          nullptr,
          nullptr,
          0,
          nullptr,
          0,
          nullptr,
          0,
          separator.data(),
          separator.size(),
          nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "paper compile plan should reject null output");
}

void test_paper_compile_defaults_and_log_summary_are_kernel_owned() {
  kernel_owned_buffer buffer{};

  require_ok_status(
      kernel_get_default_paper_template(&buffer),
      "default paper template");
  require_true(
      buffer_to_string(buffer) == "standard-thesis",
      "default paper template should be kernel-owned");
  kernel_free_buffer(&buffer);

  std::string log;
  for (int index = 1; index <= 13; ++index) {
    log += "Error line " + std::to_string(index) + "\n";
  }
  require_ok_status(
      kernel_summarize_paper_compile_log_json(log.data(), log.size(), 9, &buffer),
      "paper compile log summary");
  const std::string summary_json = buffer_to_string(buffer);
  require_true(
      summary_json.find("\"summary\":\"Error line 1\\nError line 2") != std::string::npos,
      "paper compile log summary should preserve ordered error highlights");
  require_true(
      summary_json.find("Error line 12") != std::string::npos,
      "paper compile log summary should keep the first twelve highlights");
  require_true(
      summary_json.find("Error line 13") == std::string::npos,
      "paper compile log summary should cap highlights in the kernel");
  require_true(
      summary_json.find("\"logPrefix\":\"Error lin\"") != std::string::npos,
      "paper compile log prefix should be trimmed by character limit");
  require_true(
      summary_json.find("\"truncated\":true") != std::string::npos,
      "paper compile log summary should report truncation");
  kernel_free_buffer(&buffer);

  const std::string unicode_log =
      "\xE9\x94\x99"
      "\xE8\xAF\xAF"
      "ABC\nfatal: issue\n";
  require_ok_status(
      kernel_summarize_paper_compile_log_json(
          unicode_log.data(),
          unicode_log.size(),
          3,
          &buffer),
      "paper compile unicode log summary");
  const std::string unicode_json = buffer_to_string(buffer);
  require_true(
      unicode_json.find(
          "\"logPrefix\":\"\xE9\x94\x99"
          "\xE8\xAF\xAF"
          "A\"") !=
          std::string::npos,
      "paper compile log prefix should not split UTF-8 codepoints");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_get_default_paper_template(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "default paper template should reject null output");
  require_true(
      kernel_summarize_paper_compile_log_json(nullptr, 1, 9, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "paper compile log summary should reject null non-empty log");
}

void test_pubchem_query_and_compound_normalization_are_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string query = "  Water  ";
  require_ok_status(
      kernel_normalize_pubchem_query(query.data(), query.size(), &buffer),
      "normalize PubChem query");
  require_true(
      buffer_to_string(buffer) == "Water",
      "PubChem query trimming should be kernel-owned");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_normalize_pubchem_query("   ", 3, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PubChem query normalization should reject blank input");

  const std::string formula = " H2O ";
  require_ok_status(
      kernel_build_pubchem_compound_info_json(
          "Water",
          5,
          formula.data(),
          formula.size(),
          18.015,
          1,
          0.997,
          1,
          &buffer),
      "normalize PubChem compound info");
  require_true(
      buffer_to_string(buffer) ==
          "{\"status\":\"ok\",\"name\":\"Water\",\"formula\":\"H2O\","
          "\"molecularWeight\":18.015,\"density\":0.997}",
      "PubChem compound info should normalize formula, molecular weight, and density");
  kernel_free_buffer(&buffer);

  require_ok_status(
      kernel_build_pubchem_compound_info_json("Water", 5, "H2O", 3, 18.015, 1, -1.0, 1, &buffer),
      "normalize PubChem compound info without positive density");
  require_true(
      buffer_to_string(buffer).find("\"density\":null") != std::string::npos,
      "PubChem density should be omitted when not finite and positive");
  kernel_free_buffer(&buffer);

  require_ok_status(
      kernel_build_pubchem_compound_info_json("Water", 5, nullptr, 0, 0.0, 0, 0.0, 0, &buffer),
      "PubChem not found classification");
  require_true(
      buffer_to_string(buffer) == "{\"status\":\"notFound\"}",
      "empty PubChem properties should be classified in the kernel");
  kernel_free_buffer(&buffer);

  require_ok_status(
      kernel_build_pubchem_compound_info_json("Water", 5, "H2O", 3, 18.015, 0, 0.0, 2, &buffer),
      "PubChem ambiguous classification");
  require_true(
      buffer_to_string(buffer) == "{\"status\":\"ambiguous\"}",
      "multiple PubChem properties should be classified in the kernel");
  kernel_free_buffer(&buffer);

  require_ok_status(
      kernel_build_pubchem_compound_info_json("Water", 5, "  ", 2, 18.015, 0, 0.0, 1, &buffer),
      "PubChem invalid property classification");
  require_true(
      buffer_to_string(buffer) == "{\"status\":\"notFound\"}",
      "blank PubChem formula should be classified as not found in the kernel");
  kernel_free_buffer(&buffer);
}

void test_note_display_name_derivation_is_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string markdown_path = "Folder/Alpha.md";
  require_ok_status(
      kernel_derive_note_display_name_from_path(markdown_path.data(), markdown_path.size(), &buffer),
      "derive note display name from POSIX path");
  require_true(
      buffer_to_string(buffer) == "Alpha",
      "note display name derivation should strip final extension");
  kernel_free_buffer(&buffer);

  const std::string windows_path = "Lab\\Beta.MD";
  require_ok_status(
      kernel_derive_note_display_name_from_path(windows_path.data(), windows_path.size(), &buffer),
      "derive note display name from Windows path");
  require_true(
      buffer_to_string(buffer) == "Beta",
      "note display name derivation should handle Windows separators");
  kernel_free_buffer(&buffer);

  const std::string extensionless_path = "README";
  require_ok_status(
      kernel_derive_note_display_name_from_path(
          extensionless_path.data(),
          extensionless_path.size(),
          &buffer),
      "derive note display name from extensionless path");
  require_true(
      buffer_to_string(buffer) == "README",
      "note display name derivation should preserve extensionless names");
  kernel_free_buffer(&buffer);

  const std::string dotfile_path = ".env";
  require_ok_status(
      kernel_derive_note_display_name_from_path(dotfile_path.data(), dotfile_path.size(), &buffer),
      "derive note display name from dotfile path");
  require_true(
      buffer_to_string(buffer) == ".env",
      "note display name derivation should preserve dotfiles without another dot");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_derive_note_display_name_from_path(nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "note display name derivation should reject null non-empty input");
  require_true(
      kernel_derive_note_display_name_from_path(markdown_path.data(), markdown_path.size(), nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note display name derivation should reject null output");
}

void test_semantic_context_trims_short_content() {
  const std::string content = "  short note  \n";
  kernel_owned_buffer buffer{};

  require_ok_status(
      kernel_build_semantic_context(content.data(), content.size(), &buffer),
      "semantic context short content");

  require_true(buffer_to_string(buffer) == "short note", "semantic context should trim short content");
  kernel_free_buffer(&buffer);
  require_true(buffer.data == nullptr && buffer.size == 0, "semantic context free should reset buffer");
}

void test_semantic_context_extracts_headings_and_recent_blocks() {
  const std::string content =
      "# Intro\n" + std::string(2300, 'x') +
      "\n\n## Keep 1\nalpha"
      "\n\n### Keep 2\nbeta"
      "\n\n#### Keep 3\ngamma"
      "\n\n# Keep 4\ndelta"
      "\n\nfinal block";
  const std::string expected =
      "Headings:\n"
      "## Keep 1\n"
      "### Keep 2\n"
      "#### Keep 3\n"
      "# Keep 4\n"
      "\n"
      "Recent focus:\n"
      "#### Keep 3\ngamma\n"
      "\n"
      "# Keep 4\ndelta\n"
      "\n"
      "final block";
  kernel_owned_buffer buffer{};

  require_ok_status(
      kernel_build_semantic_context(content.data(), content.size(), &buffer),
      "semantic context long content");

  require_true(buffer_to_string(buffer) == expected, "semantic context should preserve legacy focus shape");
  kernel_free_buffer(&buffer);
}

void test_semantic_context_validates_arguments() {
  const std::string content = "content";
  kernel_owned_buffer buffer{};

  require_true(
      kernel_build_semantic_context(content.data(), content.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "semantic context should reject null output");
  require_true(
      kernel_build_semantic_context(nullptr, 1, &buffer).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "semantic context should reject null non-empty content");
  require_ok_status(
      kernel_build_semantic_context(nullptr, 0, &buffer),
      "semantic context should accept empty null content");
  require_true(buffer.size == 0 && buffer.data == nullptr, "empty semantic context should return empty buffer");
}

void test_product_text_limits_are_kernel_owned() {
  size_t value = 0;

  require_ok_status(kernel_get_semantic_context_min_bytes(&value), "semantic context min bytes");
  require_true(value == 24, "semantic context min bytes should come from kernel");
  require_true(
      kernel_get_semantic_context_min_bytes(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "semantic context min bytes should reject null output");

  require_ok_status(kernel_get_rag_context_per_note_char_limit(&value), "RAG context per note chars");
  require_true(value == 1500, "RAG context per-note char limit should come from kernel");
  require_true(
      kernel_get_rag_context_per_note_char_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "RAG context per-note char limit should reject null output");

  require_ok_status(kernel_get_embedding_text_char_limit(&value), "embedding text char limit");
  require_true(value == 2000, "embedding text char limit should come from kernel");
  require_true(
      kernel_get_embedding_text_char_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text char limit should reject null output");
}

void test_ai_host_runtime_defaults_are_kernel_owned() {
  size_t value = 0;

  require_ok_status(kernel_get_ai_chat_timeout_secs(&value), "AI chat timeout seconds");
  require_true(value == 120, "AI chat timeout should come from kernel");
  require_true(
      kernel_get_ai_chat_timeout_secs(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI chat timeout should reject null output");

  require_ok_status(kernel_get_ai_ponder_timeout_secs(&value), "AI ponder timeout seconds");
  require_true(value == 60, "AI ponder timeout should come from kernel");
  require_true(
      kernel_get_ai_ponder_timeout_secs(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI ponder timeout should reject null output");

  require_ok_status(
      kernel_get_ai_embedding_request_timeout_secs(&value),
      "AI embedding request timeout seconds");
  require_true(value == 30, "AI embedding request timeout should come from kernel");
  require_true(
      kernel_get_ai_embedding_request_timeout_secs(nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "AI embedding request timeout should reject null output");

  require_ok_status(kernel_get_ai_embedding_cache_limit(&value), "AI embedding cache limit");
  require_true(value == 64, "AI embedding cache limit should come from kernel");
  require_true(
      kernel_get_ai_embedding_cache_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI embedding cache limit should reject null output");

  require_ok_status(
      kernel_get_ai_embedding_concurrency_limit(&value),
      "AI embedding concurrency limit");
  require_true(value == 4, "AI embedding concurrency limit should come from kernel");
  require_true(
      kernel_get_ai_embedding_concurrency_limit(nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "AI embedding concurrency limit should reject null output");

  require_ok_status(kernel_get_ai_rag_top_note_limit(&value), "AI RAG top note limit");
  require_true(value == 5, "AI RAG top note limit should come from kernel");
  require_true(
      kernel_get_ai_rag_top_note_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI RAG top note limit should reject null output");
}

void test_ai_embedding_text_normalization_is_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string text = "  useful text  ";
  require_ok_status(
      kernel_normalize_ai_embedding_text(text.data(), text.size(), &buffer),
      "AI embedding text normalization");
  require_true(
      buffer_to_string(buffer) == text,
      "embedding text normalization should preserve non-empty caller text shape");
  kernel_free_buffer(&buffer);

  const std::string han = utf8_string(u8"\u4F60");
  std::string long_content;
  for (std::size_t index = 0; index < 2000; ++index) {
    long_content.append(han);
  }
  long_content.push_back('Z');
  require_ok_status(
      kernel_normalize_ai_embedding_text(long_content.data(), long_content.size(), &buffer),
      "AI embedding text truncation");
  const std::string normalized = buffer_to_string(buffer);
  require_true(
      normalized.find("Z") == std::string::npos,
      "embedding text normalization should truncate at the kernel-owned Unicode char limit");
  require_true(
      normalized.size() == 2000 * han.size(),
      "embedding text normalization should preserve exactly 2000 three-byte characters");
  kernel_free_buffer(&buffer);

  const std::string whitespace = " \t\n" + utf8_string(u8"\u3000");
  require_true(
      kernel_normalize_ai_embedding_text(whitespace.data(), whitespace.size(), &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text normalization should reject all-whitespace input");
  require_true(
      kernel_normalize_ai_embedding_text(nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text normalization should reject null non-empty text");
  require_true(
      kernel_normalize_ai_embedding_text(text.data(), text.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text normalization should reject null output");
}

void test_ai_embedding_text_indexability_is_kernel_owned() {
  uint8_t indexable = 0;

  const std::string useful = "  useful text  ";
  require_ok_status(
      kernel_is_ai_embedding_text_indexable(useful.data(), useful.size(), &indexable),
      "AI embedding text indexability");
  require_true(indexable == 1, "non-empty embedding text should be indexable");

  const std::string whitespace = " \t\n" + utf8_string(u8"\u3000");
  indexable = 1;
  require_ok_status(
      kernel_is_ai_embedding_text_indexable(whitespace.data(), whitespace.size(), &indexable),
      "AI embedding whitespace indexability");
  require_true(indexable == 0, "all-whitespace embedding text should not be indexable");

  indexable = 1;
  require_ok_status(
      kernel_is_ai_embedding_text_indexable(nullptr, 0, &indexable),
      "AI embedding empty-null indexability");
  require_true(indexable == 0, "empty embedding text should not be indexable");

  const std::string truncated_whitespace = std::string(2000, ' ') + "Z";
  indexable = 1;
  require_ok_status(
      kernel_is_ai_embedding_text_indexable(
          truncated_whitespace.data(),
          truncated_whitespace.size(),
          &indexable),
      "AI embedding truncated whitespace indexability");
  require_true(
      indexable == 0,
      "embedding indexability should use the kernel-owned truncated input");

  require_true(
      kernel_is_ai_embedding_text_indexable(nullptr, 1, &indexable).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text indexability should reject null non-empty text");
  require_true(
      kernel_is_ai_embedding_text_indexable(useful.data(), useful.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text indexability should reject null output");
}

void test_ai_embedding_cache_key_is_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string base_url = "https://api.example.test";
  const std::string model = "embed-small";
  const std::string text = "normalized text";
  require_ok_status(
      kernel_compute_ai_embedding_cache_key(
          base_url.data(),
          base_url.size(),
          model.data(),
          model.size(),
          text.data(),
          text.size(),
          &buffer),
      "AI embedding cache key");
  const std::string key = buffer_to_string(buffer);
  require_true(key.size() == 16, "embedding cache key should be a 64-bit hex string");
  require_true(key == "098c1904d3c511cf", "embedding cache key should be stable");
  kernel_free_buffer(&buffer);

  const std::string other_text = "other text";
  require_ok_status(
      kernel_compute_ai_embedding_cache_key(
          base_url.data(),
          base_url.size(),
          model.data(),
          model.size(),
          other_text.data(),
          other_text.size(),
          &buffer),
      "AI embedding cache key changed text");
  require_true(
      buffer_to_string(buffer) != key,
      "embedding cache key should change when normalized text changes");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_compute_ai_embedding_cache_key(
          nullptr,
          1,
          model.data(),
          model.size(),
          text.data(),
          text.size(),
          &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding cache key should reject null non-empty base URL");
  require_true(
      kernel_compute_ai_embedding_cache_key(
          base_url.data(),
          base_url.size(),
          model.data(),
          model.size(),
          text.data(),
          text.size(),
          nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding cache key should reject null output");
}

void test_ai_embedding_blob_codec_is_kernel_owned() {
  const float values[] = {1.0F, -2.5F, 0.25F};
  kernel_owned_buffer blob{};
  require_ok_status(
      kernel_serialize_ai_embedding_blob(values, 3, &blob),
      "AI embedding blob serialization");
  const std::vector<std::uint8_t> expected_blob = {
      0x00,
      0x00,
      0x80,
      0x3f,
      0x00,
      0x00,
      0x20,
      0xc0,
      0x00,
      0x00,
      0x80,
      0x3e,
  };
  require_true(blob.size == expected_blob.size(), "embedding blob byte count");
  const auto* blob_bytes = reinterpret_cast<const std::uint8_t*>(blob.data);
  require_true(
      std::equal(blob_bytes, blob_bytes + blob.size, expected_blob.begin()),
      "embedding blob should use stable little-endian f32 bytes");

  kernel_float_buffer parsed{};
  require_ok_status(
      kernel_parse_ai_embedding_blob(blob_bytes, blob.size, &parsed),
      "AI embedding blob parse");
  require_true(parsed.count == 3, "embedding blob parse should restore value count");
  require_true(parsed.values[0] == 1.0F, "embedding blob parse should restore first value");
  require_true(parsed.values[1] == -2.5F, "embedding blob parse should restore second value");
  require_true(parsed.values[2] == 0.25F, "embedding blob parse should restore third value");
  kernel_free_float_buffer(&parsed);
  kernel_free_buffer(&blob);

  const std::uint8_t invalid_blob[] = {0x00, 0x01, 0x02};
  require_true(
      kernel_parse_ai_embedding_blob(invalid_blob, sizeof(invalid_blob), &parsed).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding blob parse should reject non-f32 byte counts");
  require_true(
      kernel_serialize_ai_embedding_blob(nullptr, 1, &blob).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding blob serialization should reject null non-empty vectors");
  require_true(
      kernel_serialize_ai_embedding_blob(values, 3, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding blob serialization should reject null output");
  require_true(
      kernel_parse_ai_embedding_blob(
          reinterpret_cast<const std::uint8_t*>(expected_blob.data()),
          expected_blob.size(),
          nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding blob parse should reject null output");
}

void test_ai_embedding_note_refresh_decision_is_kernel_owned() {
  std::uint8_t should_refresh = 0;

  require_ok_status(
      kernel_should_refresh_ai_embedding_note(200, 0, 0, &should_refresh),
      "AI embedding note refresh without existing timestamp");
  require_true(
      should_refresh == 1,
      "embedding note should refresh when the compatibility cache has no timestamp");

  should_refresh = 0;
  require_ok_status(
      kernel_should_refresh_ai_embedding_note(200, 1, 199, &should_refresh),
      "AI embedding note refresh with stale timestamp");
  require_true(
      should_refresh == 1,
      "embedding note should refresh when the note timestamp is newer than the cache");

  should_refresh = 1;
  require_ok_status(
      kernel_should_refresh_ai_embedding_note(200, 1, 200, &should_refresh),
      "AI embedding note refresh with equal timestamp");
  require_true(
      should_refresh == 0,
      "embedding note should not refresh when timestamps are equal");

  should_refresh = 1;
  require_ok_status(
      kernel_should_refresh_ai_embedding_note(199, 1, 200, &should_refresh),
      "AI embedding note refresh with newer cache timestamp");
  require_true(
      should_refresh == 0,
      "embedding note should not refresh when the cache timestamp is newer");

  require_true(
      kernel_should_refresh_ai_embedding_note(200, 1, 199, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding note refresh should reject null output");
}

void test_ai_rag_context_shape_and_truncation_are_kernel_owned() {
  kernel_owned_buffer buffer{};

  const char* names[] = {"Alpha", "Blank", "Beta"};
  const size_t name_sizes[] = {5, 5, 4};
  const char* contents[] = {"first", " \n\t ", "second"};
  const size_t content_sizes[] = {5, 4, 6};
  require_ok_status(
      kernel_build_ai_rag_context(names, name_sizes, contents, content_sizes, 3, &buffer),
      "AI RAG note context");
  const std::string expected =
      utf8_string(
          u8"--- \u7B14\u8BB0 1 \u300AAlpha\u300B ---\nfirst\n\n"
          u8"--- \u7B14\u8BB0 2 \u300ABeta\u300B ---\nsecond\n\n");
  require_true(
      buffer_to_string(buffer) == expected,
      "RAG note context should preserve kernel-owned note headers and skip blank notes");
  kernel_free_buffer(&buffer);

  const char* note_paths[] = {"Folder/Alpha.md", "Blank.md", "Lab\\Beta.MD", "README"};
  const size_t note_path_sizes[] = {15, 8, 11, 6};
  const char* path_contents[] = {"first", " \n\t ", "second", "third"};
  const size_t path_content_sizes[] = {5, 4, 6, 5};
  require_ok_status(
      kernel_build_ai_rag_context_from_note_paths(
          note_paths,
          note_path_sizes,
          path_contents,
          path_content_sizes,
          4,
          &buffer),
      "AI RAG context from note paths");
  const std::string expected_from_paths =
      utf8_string(
          u8"--- \u7B14\u8BB0 1 \u300AAlpha\u300B ---\nfirst\n\n"
          u8"--- \u7B14\u8BB0 2 \u300ABeta\u300B ---\nsecond\n\n"
          u8"--- \u7B14\u8BB0 3 \u300AREADME\u300B ---\nthird\n\n");
  require_true(
      buffer_to_string(buffer) == expected_from_paths,
      "RAG note context should derive display names from note paths in kernel");
  kernel_free_buffer(&buffer);

  require_ok_status(
      kernel_build_ai_rag_context(nullptr, nullptr, nullptr, nullptr, 0, &buffer),
      "empty AI RAG note context");
  require_true(buffer_to_string(buffer).empty(), "empty RAG note context should return empty text");
  kernel_free_buffer(&buffer);

  const std::string han = utf8_string(u8"\u4F60");
  std::string long_content;
  for (std::size_t index = 0; index < 1500; ++index) {
    long_content.append(han);
  }
  long_content.push_back('Z');
  const char* long_names[] = {"Long"};
  const size_t long_name_sizes[] = {4};
  const char* long_contents[] = {long_content.data()};
  const size_t long_content_sizes[] = {long_content.size()};
  require_ok_status(
      kernel_build_ai_rag_context(
          long_names,
          long_name_sizes,
          long_contents,
          long_content_sizes,
          1,
          &buffer),
      "AI RAG note context truncation");
  const std::string truncated = buffer_to_string(buffer);
  require_true(
      truncated.find("Z") == std::string::npos,
      "RAG note context should truncate after the kernel-owned per-note char limit");
  require_true(
      truncated.rfind("\n\n") == truncated.size() - 2,
      "RAG note context should preserve the trailing note separator");
  require_true(
      truncated.find(utf8_string(u8"--- \u7B14\u8BB0 1 \u300ALong\u300B ---\n")) == 0,
      "RAG note context should preserve the Long note header");
  kernel_free_buffer(&buffer);

  const char* invalid_names[] = {nullptr};
  const size_t invalid_name_sizes[] = {1};
  const char* empty_contents[] = {""};
  const size_t empty_content_sizes[] = {0};
  require_true(
      kernel_build_ai_rag_context(
          invalid_names,
          invalid_name_sizes,
          empty_contents,
          empty_content_sizes,
          1,
          &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "RAG note context should reject null non-empty names");
  require_true(
      kernel_build_ai_rag_context(names, name_sizes, contents, content_sizes, 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "RAG note context should reject null output");
  require_true(
      kernel_build_ai_rag_context_from_note_paths(
          invalid_names,
          invalid_name_sizes,
          empty_contents,
          empty_content_sizes,
          1,
          &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "RAG note context from paths should reject null non-empty paths");
}

void test_ai_prompt_shapes_are_kernel_owned() {
  kernel_owned_buffer buffer{};

  const std::string rag_context = "CTX";
  require_ok_status(
      kernel_build_ai_rag_system_content(
          rag_context.data(),
          rag_context.size(),
          &buffer),
      "AI RAG system content");
  const std::string rag_system_content = buffer_to_string(buffer);
  require_true(
      rag_system_content.find(rag_context) != std::string::npos,
      "RAG system content should include caller context");
  require_true(
      rag_system_content.find(utf8_string(u8"\u4EE5\u4E0B\u662F\u76F8\u5173\u7B14\u8BB0")) !=
          std::string::npos,
      "RAG system content should include the kernel-owned context header");
  kernel_free_buffer(&buffer);

  require_ok_status(kernel_get_ai_ponder_system_prompt(&buffer), "AI ponder system prompt");
  const std::string ponder_system = buffer_to_string(buffer);
  require_true(
      ponder_system.find("JSON") != std::string::npos,
      "ponder system prompt should preserve strict JSON instruction");
  require_true(
      ponder_system.find("Markdown") != std::string::npos,
      "ponder system prompt should preserve no-Markdown instruction");
  kernel_free_buffer(&buffer);

  const std::string topic = "Atom";
  const std::string context = "Bond";
  require_ok_status(
      kernel_build_ai_ponder_user_prompt(
          topic.data(),
          topic.size(),
          context.data(),
          context.size(),
          &buffer),
      "AI ponder user prompt");
  const std::string expected_user_prompt =
      utf8_string(u8"\u6838\u5FC3\u6982\u5FF5: ") + topic + "\n" +
      utf8_string(u8"\u4E0A\u4E0B\u6587: ") + context + "\n" +
      utf8_string(
          u8"\u8BF7\u751F\u6210 3 \u5230 5 \u4E2A\u5177\u5907\u903B\u8F91"
          u8"\u9012\u8FDB\u6216\u8865\u5145\u5173\u7CFB\u7684\u5B50\u8282"
          u8"\u70B9\u3002");
  require_true(
      buffer_to_string(buffer) == expected_user_prompt,
      "ponder user prompt should preserve kernel-owned prompt shape");
  kernel_free_buffer(&buffer);

  float temperature = 0.0F;
  require_ok_status(kernel_get_ai_ponder_temperature(&temperature), "AI ponder temperature");
  require_true(std::fabs(temperature - 0.7F) < 0.0001F, "ponder temperature should come from kernel");

  require_true(
      kernel_build_ai_rag_system_content(nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "RAG system content should reject null non-empty context");
  require_true(
      kernel_get_ai_ponder_system_prompt(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "ponder system prompt should reject null output");
  require_true(
      kernel_build_ai_ponder_user_prompt(nullptr, 1, context.data(), context.size(), &buffer)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "ponder user prompt should reject null non-empty topic");
  require_true(
      kernel_build_ai_ponder_user_prompt(topic.data(), topic.size(), nullptr, 1, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "ponder user prompt should reject null non-empty context");
  require_true(
      kernel_get_ai_ponder_temperature(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "ponder temperature should reject null output");
}

void test_truth_state_routes_activity_and_levels() {
  const kernel_study_note_activity activities[] = {
      {"lab.csv", 120},
      {"code.rs", 3600},
      {"molecule.mol", 3000},
      {"ledger.base", 6000},
  };
  kernel_truth_state_snapshot state{};

  require_ok_status(
      kernel_compute_truth_state_from_activity(activities, 4, &state),
      "truth state from activity");

  require_true(state.level == 2, "truth state should compute overall level");
  require_true(state.total_exp == 112, "truth state should carry remaining level exp");
  require_true(state.next_level_exp == 150, "truth state should compute next level requirement");
  require_true(state.attribute_exp.science == 2, "csv activity routes to science exp");
  require_true(state.attribute_exp.engineering == 60, "rs activity routes to engineering exp");
  require_true(state.attribute_exp.creation == 50, "mol activity routes to creation exp");
  require_true(state.attribute_exp.finance == 100, "base activity routes to finance exp");
  require_true(state.attributes.science == 1, "science attribute level should use kernel curve");
  require_true(state.attributes.engineering == 2, "engineering attribute level should use kernel curve");
  require_true(state.attributes.creation == 2, "creation attribute level should use kernel curve");
  require_true(state.attributes.finance == 3, "finance attribute level should use kernel curve");
}

void test_truth_state_validates_arguments() {
  kernel_truth_state_snapshot state{};
  const kernel_study_note_activity invalid[] = {{nullptr, 60}};

  require_ok_status(
      kernel_compute_truth_state_from_activity(nullptr, 0, &state),
      "truth state should accept empty activity");
  require_true(state.level == 1, "empty truth state should start at level one");
  require_true(state.next_level_exp == 100, "empty truth state should keep level one requirement");
  require_true(
      kernel_compute_truth_state_from_activity(nullptr, 1, &state).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth state should reject null non-empty activity buffer");
  require_true(
      kernel_compute_truth_state_from_activity(invalid, 1, &state).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth state should reject null note ids");
  require_true(
      kernel_compute_truth_state_from_activity(nullptr, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth state should reject null output");
}

void test_study_stats_window_computes_legacy_boundaries() {
  kernel_study_stats_window window{};

  require_ok_status(
      kernel_compute_study_stats_window(1714305600, 7, &window),
      "study stats window");

  require_true(window.today_start_epoch_secs == 1714262400, "stats window should floor now to UTC day");
  require_true(window.today_bucket == 19841, "stats window should expose today bucket");
  require_true(window.week_start_epoch_secs == 1713744000, "stats window should preserve 6-day week lookback");
  require_true(window.daily_window_start_epoch_secs == 1713744000, "stats window should include days_back days");
  require_true(window.heatmap_start_epoch_secs == 1698796800, "stats window should preserve legacy heatmap lookback");
  require_true(window.folder_rank_limit == 5, "stats window should own folder ranking limit");
}

void test_study_stats_window_validates_arguments() {
  kernel_study_stats_window window{};

  require_true(
      kernel_compute_study_stats_window(1714305600, 0, &window).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "stats window should reject non-positive days_back");
  require_true(
      kernel_compute_study_stats_window(1714305600, 7, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "stats window should reject null output");
}

void test_study_streak_counts_contiguous_active_days() {
  const int64_t buckets[] = {12, 10, 9, 10, 8, 2};
  int64_t streak = 0;

  require_ok_status(
      kernel_compute_study_streak_days(buckets, 6, 10, &streak),
      "study streak");

  require_true(streak == 3, "streak should count contiguous days through today");

  require_ok_status(
      kernel_compute_study_streak_days(buckets, 6, 11, &streak),
      "study streak missing today");
  require_true(streak == 0, "streak should be zero when today has no activity");
}

void test_study_streak_buckets_timestamps_in_kernel() {
  const int64_t timestamps[] = {
      10 * 86400 + 5,
      9 * 86400 + 120,
      8 * 86400,
      10 * 86400 + 400,
      12 * 86400,
      -1,
  };
  int64_t streak = 0;

  require_ok_status(
      kernel_compute_study_streak_days_from_timestamps(timestamps, 6, 10, &streak),
      "study streak from timestamps");
  require_true(streak == 3, "timestamp streak should bucket and dedupe active days");

  require_ok_status(
      kernel_compute_study_streak_days_from_timestamps(timestamps, 6, -1, &streak),
      "study streak from negative timestamp");
  require_true(streak == 1, "timestamp bucketing should floor negative epoch seconds");
}

void test_study_streak_validates_arguments() {
  int64_t streak = 0;

  require_ok_status(
      kernel_compute_study_streak_days(nullptr, 0, 10, &streak),
      "empty study streak");
  require_true(streak == 0, "empty streak should be zero");
  require_true(
      kernel_compute_study_streak_days(nullptr, 1, 10, &streak).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "streak should reject null non-empty bucket buffer");
  require_true(
      kernel_compute_study_streak_days(nullptr, 0, 10, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "streak should reject null output");
  require_true(
      kernel_compute_study_streak_days_from_timestamps(nullptr, 1, 10, &streak).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "timestamp streak should reject null non-empty timestamp buffer");
  require_true(
      kernel_compute_study_streak_days_from_timestamps(nullptr, 0, 10, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "timestamp streak should reject null output");
}

std::string heatmap_date_at(const kernel_heatmap_grid& grid, const size_t index) {
  return grid.cells[index].date == nullptr ? std::string() : std::string(grid.cells[index].date);
}

void test_study_heatmap_grid_builds_fixed_monday_aligned_grid() {
  const kernel_heatmap_day_activity days[] = {
      {"2023-10-30", 60},
      {"2024-01-01", 120},
      {"2024-01-01", 30},
      {"2024-04-28", 300},
      {"2022-01-01", 999},
  };
  kernel_heatmap_grid grid{};

  require_ok_status(
      kernel_build_study_heatmap_grid(days, 5, 1714305600, &grid),
      "study heatmap grid");

  require_true(grid.weeks == 26, "study heatmap grid should own week count");
  require_true(grid.days_per_week == 7, "study heatmap grid should own days per week");
  require_true(grid.count == 182, "study heatmap grid should return 26x7 cells");
  require_true(grid.max_secs == 300, "study heatmap grid should compute max seconds");
  require_true(heatmap_date_at(grid, 0) == "2023-10-30", "heatmap should start on Monday");
  require_true(grid.cells[0].secs == 60, "first cell should include matching activity");
  require_true(grid.cells[0].col == 0 && grid.cells[0].row == 0, "first cell coordinates");
  require_true(heatmap_date_at(grid, 63) == "2024-01-01", "duplicate activity date should be in grid");
  require_true(grid.cells[63].secs == 150, "duplicate activity dates should be summed");
  require_true(grid.cells[63].col == 9 && grid.cells[63].row == 0, "January first cell coordinates");
  require_true(heatmap_date_at(grid, 181) == "2024-04-28", "heatmap should end on today");
  require_true(grid.cells[181].secs == 300, "last cell should include today activity");
  require_true(grid.cells[181].col == 25 && grid.cells[181].row == 6, "last cell coordinates");

  kernel_free_study_heatmap_grid(&grid);
  require_true(grid.cells == nullptr && grid.count == 0, "heatmap free should reset grid");
}

void test_study_heatmap_grid_validates_arguments() {
  kernel_heatmap_grid grid{};
  const kernel_heatmap_day_activity invalid[] = {{nullptr, 60}};

  require_true(
      kernel_build_study_heatmap_grid(nullptr, 1, 1714305600, &grid).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "heatmap grid should reject null non-empty day buffer");
  require_true(
      kernel_build_study_heatmap_grid(invalid, 1, 1714305600, &grid).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "heatmap grid should reject null dates");
  require_true(
      kernel_build_study_heatmap_grid(nullptr, 0, 1714305600, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "heatmap grid should reject null output");
}

}  // namespace

void run_product_compute_tests() {
  test_truth_diff_text_delta_routes_by_extension();
  test_truth_diff_code_language_award();
  test_truth_diff_molecular_line_award();
  test_truth_diff_empty_and_invalid_args();
  test_truth_award_reason_keys_are_kernel_owned();
  test_file_extension_derivation_is_kernel_owned();
  test_database_column_type_normalization_is_kernel_owned();
  test_database_payload_normalization_json_is_kernel_owned();
  test_paper_compile_plan_json_is_kernel_owned();
  test_paper_compile_defaults_and_log_summary_are_kernel_owned();
  test_pubchem_query_and_compound_normalization_are_kernel_owned();
  test_note_display_name_derivation_is_kernel_owned();
  test_semantic_context_trims_short_content();
  test_semantic_context_extracts_headings_and_recent_blocks();
  test_semantic_context_validates_arguments();
  test_product_text_limits_are_kernel_owned();
  test_ai_host_runtime_defaults_are_kernel_owned();
  test_ai_embedding_text_normalization_is_kernel_owned();
  test_ai_embedding_text_indexability_is_kernel_owned();
  test_ai_embedding_cache_key_is_kernel_owned();
  test_ai_embedding_blob_codec_is_kernel_owned();
  test_ai_embedding_note_refresh_decision_is_kernel_owned();
  test_ai_rag_context_shape_and_truncation_are_kernel_owned();
  test_ai_prompt_shapes_are_kernel_owned();
  test_truth_state_routes_activity_and_levels();
  test_truth_state_validates_arguments();
  test_study_stats_window_computes_legacy_boundaries();
  test_study_stats_window_validates_arguments();
  test_study_streak_counts_contiguous_active_days();
  test_study_streak_buckets_timestamps_in_kernel();
  test_study_streak_validates_arguments();
  test_study_heatmap_grid_builds_fixed_monday_aligned_grid();
  test_study_heatmap_grid_validates_arguments();
}
