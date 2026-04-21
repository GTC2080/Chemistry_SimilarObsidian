// Reason: This file provides a default no-op logger so the skeleton can compile without a logging backend.

#include "diagnostics/logger.h"

namespace kernel::diagnostics {

class NullLogger final : public Logger {
 public:
  std::string_view backend_name() const override { return "null_logger"; }
  void info(std::string_view) override {}
  void error(std::string_view) override {}
};

std::unique_ptr<Logger> make_null_logger() {
  return std::make_unique<NullLogger>();
}

}  // namespace kernel::diagnostics
