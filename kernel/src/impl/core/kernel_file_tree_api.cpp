// Reason: This file owns host-facing file tree construction so Tauri Rust
// consumes a kernel-derived vault hierarchy instead of rebuilding it itself.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kDefaultFileTreeLimit = 4096;

struct TreeNote {
  std::string rel_path;
  std::string name;
  std::string extension;
  std::uint64_t mtime_ns = 0;
};

struct TreeNode {
  std::string name;
  std::string full_name;
  std::string relative_path;
  bool is_folder = false;
  std::uint32_t file_count = 0;
  bool has_note = false;
  TreeNote note;
  std::vector<TreeNode> children;
};

std::string normalize_rel_path(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  return value;
}

std::vector<std::string> split_rel_path(const std::string& rel_path) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= rel_path.size()) {
    const std::size_t slash = rel_path.find('/', start);
    const std::size_t end = slash == std::string::npos ? rel_path.size() : slash;
    if (end > start) {
      parts.push_back(rel_path.substr(start, end - start));
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }
  return parts;
}

std::string trim_ignored_root(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }
  while (start < value.size() && (value[start] == '/' || value[start] == '\\')) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  while (end > start && (value[end - 1] == '/' || value[end - 1] == '\\')) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

std::set<std::string> parse_ignored_roots(const char* ignored_roots_csv) {
  std::set<std::string> ignored;
  if (ignored_roots_csv == nullptr || ignored_roots_csv[0] == '\0') {
    return ignored;
  }

  const std::string_view raw(ignored_roots_csv);
  std::size_t start = 0;
  while (start <= raw.size()) {
    const std::size_t next = raw.find(',', start);
    const std::string root = trim_ignored_root(
        next == std::string_view::npos ? raw.substr(start) : raw.substr(start, next - start));
    if (!root.empty()) {
      ignored.insert(root);
    }
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 1;
  }
  return ignored;
}

std::string first_rel_path_segment(const std::string& rel_path) {
  const std::size_t slash = rel_path.find('/');
  return slash == std::string::npos ? rel_path : rel_path.substr(0, slash);
}

std::string basename_stem(const std::string& basename) {
  const std::size_t dot = basename.find_last_of('.');
  if (dot == std::string::npos || dot == 0) {
    return basename;
  }
  return basename.substr(0, dot);
}

std::string lowercase_extension(const std::string& basename) {
  const std::size_t dot = basename.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= basename.size()) {
    return {};
  }
  std::string extension = basename.substr(dot + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return extension;
}

TreeNode make_folder_node(const std::string& segment, const std::string& rel_path) {
  TreeNode node{};
  node.name = segment;
  node.full_name = segment;
  node.relative_path = rel_path;
  node.is_folder = true;
  return node;
}

TreeNode make_file_node(const kernel_note_record& note, const std::string& rel_path) {
  const std::vector<std::string> parts = split_rel_path(rel_path);
  const std::string basename = parts.empty() ? rel_path : parts.back();

  TreeNode node{};
  node.name = basename_stem(basename);
  node.full_name = basename;
  node.relative_path = rel_path;
  node.is_folder = false;
  node.file_count = 1;
  node.has_note = true;
  node.note.rel_path = rel_path;
  node.note.name = node.name;
  node.note.extension = lowercase_extension(basename);
  node.note.mtime_ns = note.mtime_ns;
  return node;
}

TreeNode* find_folder(std::vector<TreeNode>& nodes, const std::string& segment) {
  for (auto& node : nodes) {
    if (node.is_folder && node.name == segment) {
      return &node;
    }
  }
  return nullptr;
}

void insert_note(std::vector<TreeNode>& root, const kernel_note_record& note) {
  const std::string rel_path =
      normalize_rel_path(note.rel_path == nullptr ? std::string{} : std::string(note.rel_path));
  const std::vector<std::string> parts = split_rel_path(rel_path);
  if (parts.empty()) {
    return;
  }

  std::vector<TreeNode>* current = &root;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    const bool is_last = index + 1 == parts.size();
    const std::string node_rel_path = [&]() {
      std::string value;
      for (std::size_t part_index = 0; part_index <= index; ++part_index) {
        if (!value.empty()) {
          value += "/";
        }
        value += parts[part_index];
      }
      return value;
    }();

    if (is_last) {
      current->push_back(make_file_node(note, node_rel_path));
      continue;
    }

    TreeNode* folder = find_folder(*current, parts[index]);
    if (folder == nullptr) {
      current->push_back(make_folder_node(parts[index], node_rel_path));
      folder = &current->back();
    }
    current = &folder->children;
  }
}

std::uint32_t sort_and_count(std::vector<TreeNode>& nodes) {
  std::sort(nodes.begin(), nodes.end(), [](const TreeNode& left, const TreeNode& right) {
    if (left.is_folder != right.is_folder) {
      return left.is_folder;
    }
    return left.name < right.name;
  });

  std::uint32_t total = 0;
  for (auto& node : nodes) {
    if (node.is_folder) {
      node.file_count = sort_and_count(node.children);
    } else {
      node.file_count = 1;
    }
    total += node.file_count;
  }
  return total;
}

std::vector<TreeNode> build_tree(const kernel_note_list& notes, const std::set<std::string>& ignored_roots) {
  std::vector<TreeNode> root;
  for (std::size_t index = 0; index < notes.count; ++index) {
    const std::string rel_path =
        normalize_rel_path(notes.notes[index].rel_path == nullptr ? std::string{} : std::string(notes.notes[index].rel_path));
    if (!ignored_roots.empty() && ignored_roots.contains(first_rel_path_segment(rel_path))) {
      continue;
    }
    insert_note(root, notes.notes[index]);
  }
  sort_and_count(root);
  return root;
}

void free_file_tree_note(kernel_file_tree_note* note) {
  if (note == nullptr) {
    return;
  }
  delete[] note->rel_path;
  delete[] note->name;
  delete[] note->extension;
  note->rel_path = nullptr;
  note->name = nullptr;
  note->extension = nullptr;
  note->mtime_ns = 0;
}

void free_file_tree_node(kernel_file_tree_node* node) {
  if (node == nullptr) {
    return;
  }
  delete[] node->name;
  delete[] node->full_name;
  delete[] node->relative_path;
  node->name = nullptr;
  node->full_name = nullptr;
  node->relative_path = nullptr;
  free_file_tree_note(&node->note);
  if (node->children != nullptr) {
    for (std::size_t index = 0; index < node->child_count; ++index) {
      free_file_tree_node(&node->children[index]);
    }
    delete[] node->children;
  }
  node->children = nullptr;
  node->child_count = 0;
  node->file_count = 0;
  node->is_folder = 0;
  node->has_note = 0;
}

void reset_file_tree(kernel_file_tree* tree) {
  if (tree == nullptr) {
    return;
  }
  if (tree->nodes != nullptr) {
    for (std::size_t index = 0; index < tree->count; ++index) {
      free_file_tree_node(&tree->nodes[index]);
    }
    delete[] tree->nodes;
  }
  tree->nodes = nullptr;
  tree->count = 0;
}

bool fill_file_tree_note(const TreeNote& source, kernel_file_tree_note* target) {
  target->rel_path = kernel::core::duplicate_c_string(source.rel_path);
  target->name = kernel::core::duplicate_c_string(source.name);
  target->extension = kernel::core::duplicate_c_string(source.extension);
  target->mtime_ns = source.mtime_ns;
  return target->rel_path != nullptr && target->name != nullptr && target->extension != nullptr;
}

bool fill_file_tree_nodes(const std::vector<TreeNode>& source, kernel_file_tree_node** out_nodes) {
  *out_nodes = nullptr;
  if (source.empty()) {
    return true;
  }

  auto* nodes = new (std::nothrow) kernel_file_tree_node[source.size()]{};
  if (nodes == nullptr) {
    return false;
  }
  *out_nodes = nodes;

  for (std::size_t index = 0; index < source.size(); ++index) {
    const auto& source_node = source[index];
    auto& target = nodes[index];
    target.name = kernel::core::duplicate_c_string(source_node.name);
    target.full_name = kernel::core::duplicate_c_string(source_node.full_name);
    target.relative_path = kernel::core::duplicate_c_string(source_node.relative_path);
    target.is_folder = source_node.is_folder ? 1 : 0;
    target.has_note = source_node.has_note ? 1 : 0;
    target.file_count = source_node.file_count;
    target.child_count = source_node.children.size();
    if (target.name == nullptr || target.full_name == nullptr || target.relative_path == nullptr) {
      return false;
    }
    if (source_node.has_note && !fill_file_tree_note(source_node.note, &target.note)) {
      return false;
    }
    if (!fill_file_tree_nodes(source_node.children, &target.children)) {
      return false;
    }
  }
  return true;
}

}  // namespace

extern "C" kernel_status kernel_get_file_tree_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultFileTreeLimit;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_file_tree_impl(
    kernel_handle* handle,
    const size_t limit,
    const char* ignored_roots_csv,
    kernel_file_tree* out_tree) {
  reset_file_tree(out_tree);
  if (handle == nullptr || out_tree == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_note_list notes{};
  const kernel_status note_status = kernel_query_notes(handle, limit, &notes);
  if (note_status.code != KERNEL_OK) {
    return note_status;
  }

  const std::set<std::string> ignored_roots = parse_ignored_roots(ignored_roots_csv);
  const std::vector<TreeNode> tree = build_tree(notes, ignored_roots);
  kernel_free_note_list(&notes);
  out_tree->count = tree.size();
  if (!fill_file_tree_nodes(tree, &out_tree->nodes)) {
    reset_file_tree(out_tree);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_query_file_tree(
    kernel_handle* handle,
    const size_t limit,
    kernel_file_tree* out_tree) {
  return query_file_tree_impl(handle, limit, nullptr, out_tree);
}

extern "C" kernel_status kernel_query_file_tree_filtered(
    kernel_handle* handle,
    const size_t limit,
    const char* ignored_roots_csv,
    kernel_file_tree* out_tree) {
  return query_file_tree_impl(handle, limit, ignored_roots_csv, out_tree);
}

extern "C" void kernel_free_file_tree(kernel_file_tree* tree) {
  reset_file_tree(tree);
}
