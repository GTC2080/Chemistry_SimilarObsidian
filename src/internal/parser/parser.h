// Reason: This file defines the minimal internal markdown scan result for Phase 1 parser work.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace kernel::parser {

struct ParseResult {
  std::string title;
  std::vector<std::string> tags;
  std::vector<std::string> wikilinks;
  std::vector<std::string> attachment_refs;
};

ParseResult parse_markdown(std::string_view markdown);

}  // namespace kernel::parser
