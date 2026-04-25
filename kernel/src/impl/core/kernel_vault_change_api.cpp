// Reason: This file owns host-facing vault change path normalization so Rust
// does not rebuild incremental scan rules outside the kernel boundary.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"

#include <algorithm>
#include <cctype>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string trim(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

std::string normalize_change_path(std::string_view value) {
  std::string normalized = trim(value);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  return normalized;
}

std::string basename(std::string_view rel_path) {
  const std::size_t slash = rel_path.find_last_of('/');
  if (slash == std::string_view::npos) {
    return std::string(rel_path);
  }
  return std::string(rel_path.substr(slash + 1));
}

bool is_markdown_path(std::string_view rel_path) {
  const std::string leaf = basename(rel_path);
  const std::size_t dot = leaf.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= leaf.size()) {
    return false;
  }

  std::string extension = leaf.substr(dot + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return extension == "md";
}

bool is_supported_vault_event_path(std::string_view rel_path) {
  const std::string leaf = basename(rel_path);
  const std::size_t dot = leaf.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= leaf.size()) {
    return false;
  }

  std::string extension = leaf.substr(dot + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });

  static const std::set<std::string> supported_extensions = {
      "md",   "txt", "json", "py",      "rs",  "js",  "ts",  "jsx",
      "tsx",  "css", "html", "toml",    "yaml", "yml", "xml", "sh",
      "bat",  "c",   "cpp",  "h",       "java", "go",  "png", "jpg",
      "jpeg", "gif", "svg",  "webp",    "bmp", "ico", "pdf", "mol",
      "chemdraw",    "paper", "csv",    "jdx", "pdb", "xyz", "cif"};
  return supported_extensions.find(extension) != supported_extensions.end();
}

std::vector<std::string> split_lf_paths(
    const char* changed_paths_lf,
    bool (*keep_path)(std::string_view)) {
  std::vector<std::string> paths;
  if (changed_paths_lf == nullptr || changed_paths_lf[0] == '\0') {
    return paths;
  }

  const std::string_view raw(changed_paths_lf);
  std::size_t start = 0;
  while (start <= raw.size()) {
    const std::size_t next = raw.find('\n', start);
    const std::string normalized =
        normalize_change_path(next == std::string_view::npos
                                  ? raw.substr(start)
                                  : raw.substr(start, next - start));
    if (!normalized.empty() && keep_path(normalized)) {
      paths.push_back(normalized);
    }
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 1;
  }
  return paths;
}

void reset_path_list(kernel_path_list* paths) {
  if (paths == nullptr) {
    return;
  }

  if (paths->paths != nullptr) {
    for (std::size_t index = 0; index < paths->count; ++index) {
      delete[] paths->paths[index];
    }
    delete[] paths->paths;
  }

  paths->paths = nullptr;
  paths->count = 0;
}

kernel_status fill_path_list(const std::vector<std::string>& paths, kernel_path_list* out_paths) {
  if (paths.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto** owned_paths = new (std::nothrow) char*[paths.size()] {};
  if (owned_paths == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  out_paths->paths = owned_paths;
  out_paths->count = paths.size();
  for (std::size_t index = 0; index < paths.size(); ++index) {
    out_paths->paths[index] = kernel::core::duplicate_c_string(paths[index]);
    if (out_paths->paths[index] == nullptr) {
      reset_path_list(out_paths);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status filter_paths(
    const char* changed_paths_lf,
    kernel_path_list* out_paths,
    bool (*keep_path)(std::string_view)) {
  reset_path_list(out_paths);
  if (out_paths == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::set<std::string> seen;
  std::vector<std::string> filtered;
  for (const auto& path : split_lf_paths(changed_paths_lf, keep_path)) {
    if (seen.insert(path).second) {
      filtered.push_back(path);
    }
  }

  return fill_path_list(filtered, out_paths);
}

}  // namespace

extern "C" kernel_status kernel_filter_changed_markdown_paths(
    const char* changed_paths_lf,
    kernel_path_list* out_paths) {
  return filter_paths(changed_paths_lf, out_paths, is_markdown_path);
}

extern "C" kernel_status kernel_filter_supported_vault_paths(
    const char* changed_paths_lf,
    kernel_path_list* out_paths) {
  return filter_paths(changed_paths_lf, out_paths, is_supported_vault_event_path);
}

extern "C" void kernel_free_path_list(kernel_path_list* paths) {
  reset_path_list(paths);
}
