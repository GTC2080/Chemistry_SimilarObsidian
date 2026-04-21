// Reason: This file locks the minimal parser contract before it is wired into storage or indexing.

#include "parser/parser.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require_true(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error("requirement failed: " + std::string(message));
  }
}

void test_extracts_first_atx_heading_as_title() {
  const auto result = kernel::parser::parse_markdown(
      "# Title\n"
      "body\n"
      "## Subtitle\n");

  require_true(result.title == "Title", "parser should use the first ATX heading as title");
}

void test_extracts_tags_without_treating_heading_marker_as_tag() {
  const auto result = kernel::parser::parse_markdown(
      "# Heading\n"
      "We study #chem and #org_1 in notes.\n");

  require_true(result.tags.size() == 2, "parser should collect two inline tags");
  require_true(result.tags[0] == "chem", "first tag should be chem");
  require_true(result.tags[1] == "org_1", "second tag should be org_1");
}

void test_extracts_wikilinks_and_strips_alias() {
  const auto result = kernel::parser::parse_markdown(
      "See [[Page One]] and [[Target|Shown Alias]].\n");

  require_true(result.wikilinks.size() == 2, "parser should collect two wikilinks");
  require_true(result.wikilinks[0] == "Page One", "first wikilink should preserve the raw target");
  require_true(result.wikilinks[1] == "Target", "aliased wikilink should store only the target");
}

void test_trims_crlf_title_line() {
  const auto result = kernel::parser::parse_markdown(
      "# Title With CRLF\r\n"
      "body\r\n");

  require_true(result.title == "Title With CRLF", "parser should strip trailing carriage return from title");
}

void test_trims_wikilink_target_whitespace() {
  const auto result = kernel::parser::parse_markdown(
      "See [[  Target Page  |  Alias  ]].\n");

  require_true(result.wikilinks.size() == 1, "parser should collect one wikilink");
  require_true(result.wikilinks[0] == "Target Page", "parser should trim wikilink target whitespace");
}

void test_preserves_tag_order_and_duplicates() {
  const auto result = kernel::parser::parse_markdown(
      "Mix #chem then #org and #chem again.\n");

  require_true(result.tags.size() == 3, "parser should preserve duplicate tags");
  require_true(result.tags[0] == "chem", "first tag should preserve source order");
  require_true(result.tags[1] == "org", "second tag should preserve source order");
  require_true(result.tags[2] == "chem", "third tag should preserve duplicate occurrence");
}

void test_preserves_wikilink_order_and_duplicates() {
  const auto result = kernel::parser::parse_markdown(
      "[[Alpha]] then [[Beta]] then [[Alpha|Shown Again]].\n");

  require_true(result.wikilinks.size() == 3, "parser should preserve duplicate wikilinks");
  require_true(result.wikilinks[0] == "Alpha", "first wikilink should preserve source order");
  require_true(result.wikilinks[1] == "Beta", "second wikilink should preserve source order");
  require_true(result.wikilinks[2] == "Alpha", "third wikilink should preserve duplicate occurrence");
}

void test_extracts_attachment_refs_from_local_links_and_embeds() {
  const auto result = kernel::parser::parse_markdown(
      "![Figure](assets/diagram.png)\n"
      "[Paper](docs/paper.pdf)\n"
      "![[slides/deck.pptx]]\n"
      "[Site](https://example.com)\n"
      "[Local Note](local-note.md)\n");

  require_true(result.attachment_refs.size() == 3, "parser should collect three local attachment refs");
  require_true(result.attachment_refs[0] == "assets/diagram.png", "markdown image path should be captured");
  require_true(result.attachment_refs[1] == "docs/paper.pdf", "markdown local file link should be captured");
  require_true(result.attachment_refs[2] == "slides/deck.pptx", "obsidian embed path should be captured");
}

void test_empty_input_yields_empty_result() {
  const auto result = kernel::parser::parse_markdown("");

  require_true(result.title.empty(), "empty input should have no title");
  require_true(result.tags.empty(), "empty input should have no tags");
  require_true(result.wikilinks.empty(), "empty input should have no wikilinks");
  require_true(result.attachment_refs.empty(), "empty input should have no attachment refs");
}

}  // namespace

int main() {
  try {
    test_extracts_first_atx_heading_as_title();
    test_extracts_tags_without_treating_heading_marker_as_tag();
    test_extracts_wikilinks_and_strips_alias();
    test_trims_crlf_title_line();
    test_trims_wikilink_target_whitespace();
    test_preserves_tag_order_and_duplicates();
    test_preserves_wikilink_order_and_duplicates();
    test_extracts_attachment_refs_from_local_links_and_embeds();
    test_empty_input_yields_empty_result();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "parser_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
