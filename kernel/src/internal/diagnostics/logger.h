// Reason: This file defines the smallest logging seam so the kernel can log without committing to a sink.

#pragma once

#include <memory>
#include <string_view>

namespace kernel::diagnostics {

class Logger {
 public:
  virtual ~Logger() = default;

  virtual std::string_view backend_name() const = 0;
  virtual void info(std::string_view message) = 0;
  virtual void error(std::string_view message) = 0;
};

std::unique_ptr<Logger> make_null_logger();

}  // namespace kernel::diagnostics
