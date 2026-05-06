#pragma once

#include "sealed_kernel_bridge.h"

#include "kernel/c_api.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

struct sealed_kernel_bridge_session {
  kernel_handle* handle = nullptr;
};

namespace sealed_kernel_bridge_internal {

inline char* CopyString(const std::string& value) {
  auto* out = static_cast<char*>(std::malloc(value.size() + 1));
  if (out == nullptr) {
    return nullptr;
  }
  std::memcpy(out, value.c_str(), value.size() + 1);
  return out;
}

inline uint8_t* CopyBytes(const void* value, const std::size_t size) {
  if (size == 0) {
    return nullptr;
  }
  auto* out = static_cast<uint8_t*>(std::malloc(size));
  if (out == nullptr) {
    return nullptr;
  }
  std::memcpy(out, value, size);
  return out;
}

inline float* CopyFloatArray(const float* values, const std::size_t count) {
  if (count == 0) {
    return nullptr;
  }
  if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(float)) {
    return nullptr;
  }
  auto* out = static_cast<float*>(std::malloc(count * sizeof(float)));
  if (out == nullptr) {
    return nullptr;
  }
  std::memcpy(out, values, count * sizeof(float));
  return out;
}

inline void SetError(char** out_error, const std::string& message) {
  if (out_error == nullptr) {
    return;
  }
  *out_error = CopyString(message);
}

inline const char* KernelErrorCodeName(const kernel_error_code code) {
  switch (code) {
    case KERNEL_OK:
      return "KERNEL_OK";
    case KERNEL_ERROR_INVALID_ARGUMENT:
      return "KERNEL_ERROR_INVALID_ARGUMENT";
    case KERNEL_ERROR_NOT_FOUND:
      return "KERNEL_ERROR_NOT_FOUND";
    case KERNEL_ERROR_CONFLICT:
      return "KERNEL_ERROR_CONFLICT";
    case KERNEL_ERROR_IO:
      return "KERNEL_ERROR_IO";
    case KERNEL_ERROR_INTERNAL:
      return "KERNEL_ERROR_INTERNAL";
    case KERNEL_ERROR_TIMEOUT:
      return "KERNEL_ERROR_TIMEOUT";
  }

  return "KERNEL_ERROR_UNKNOWN";
}

inline std::string Utf8ToActiveCodePage(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return {};
  }

#ifdef _WIN32
  const int wide_size =
      MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
  if (wide_size <= 0) {
    return {};
  }

  std::wstring wide_value(static_cast<std::size_t>(wide_size), L'\0');
  MultiByteToWideChar(
      CP_UTF8,
      0,
      value,
      -1,
      wide_value.data(),
      wide_size);

  const int acp_size =
      WideCharToMultiByte(CP_ACP, 0, wide_value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (acp_size <= 0) {
    return {};
  }

  std::string acp_value(static_cast<std::size_t>(acp_size), '\0');
  WideCharToMultiByte(
      CP_ACP,
      0,
      wide_value.c_str(),
      -1,
      acp_value.data(),
      acp_size,
      nullptr,
      nullptr);
  if (!acp_value.empty() && acp_value.back() == '\0') {
    acp_value.pop_back();
  }
  return acp_value;
#else
  return std::string(value);
#endif
}

inline std::string Utf8ToActiveCodePage(const char* value, const uint64_t value_size) {
  if (value == nullptr || value_size == 0 ||
      value_size > static_cast<uint64_t>((std::numeric_limits<int>::max)())) {
    return {};
  }

#ifdef _WIN32
  const int input_size = static_cast<int>(value_size);
  const int wide_size =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, input_size, nullptr, 0);
  if (wide_size <= 0) {
    return {};
  }

  std::wstring wide_value(static_cast<std::size_t>(wide_size), L'\0');
  MultiByteToWideChar(
      CP_UTF8,
      MB_ERR_INVALID_CHARS,
      value,
      input_size,
      wide_value.data(),
      wide_size);

  const int acp_size = WideCharToMultiByte(
      CP_ACP,
      0,
      wide_value.data(),
      wide_size,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (acp_size <= 0) {
    return {};
  }

  std::string acp_value(static_cast<std::size_t>(acp_size), '\0');
  WideCharToMultiByte(
      CP_ACP,
      0,
      wide_value.data(),
      wide_size,
      acp_value.data(),
      acp_size,
      nullptr,
      nullptr);
  return acp_value;
#else
  return std::string(value, static_cast<std::size_t>(value_size));
#endif
}

inline std::string ActiveCodePageToUtf8(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return {};
  }

#ifdef _WIN32
  const int wide_size =
      MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
  if (wide_size <= 0) {
    return {};
  }

  std::wstring wide_value(static_cast<std::size_t>(wide_size), L'\0');
  MultiByteToWideChar(CP_ACP, 0, value, -1, wide_value.data(), wide_size);

  const int utf8_size =
      WideCharToMultiByte(CP_UTF8, 0, wide_value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8_size <= 0) {
    return {};
  }

  std::string utf8_value(static_cast<std::size_t>(utf8_size), '\0');
  WideCharToMultiByte(
      CP_UTF8,
      0,
      wide_value.c_str(),
      -1,
      utf8_value.data(),
      utf8_size,
      nullptr,
      nullptr);
  if (!utf8_value.empty() && utf8_value.back() == '\0') {
    utf8_value.pop_back();
  }
  return utf8_value;
#else
  return std::string(value);
#endif
}

inline int32_t ReturnKernelError(
    const kernel_status status,
    const char* operation,
    char** out_error) {
  const char* code_name = KernelErrorCodeName(status.code);
  SetError(out_error, std::string(operation) + " failed (" + code_name + ").");
  return static_cast<int32_t>(status.code);
}

inline std::string JsonEscape(const char* value) {
  if (value == nullptr) {
    return "";
  }

  std::string escaped;
  for (const unsigned char ch : std::string(value)) {
    switch (ch) {
      case '\"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (ch < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped += hex[(ch >> 4) & 0x0F];
          escaped += hex[ch & 0x0F];
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return escaped;
}

inline std::string PathListToJson(const kernel_path_list& paths) {
  std::string json = "{\"paths\":[";
  for (size_t index = 0; index < paths.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const std::string path_utf8 = ActiveCodePageToUtf8(paths.paths[index]);
    json += "\"" + JsonEscape(path_utf8.c_str()) + "\"";
  }
  json += "],\"count\":" + std::to_string(paths.count) + "}";
  return json;
}

inline void AppendNoteHitJson(
    std::string& json,
    const char* rel_path,
    const char* title,
    uint64_t mtime_ns = 0) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(rel_path);
  json += "{\"rel_path\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"title\":\"" + JsonEscape(title) + "\",";
  json += "\"mtime_ns\":" + std::to_string(mtime_ns) + "}";
}

inline std::string NoteCatalogJson(const kernel_note_list& notes) {
  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < notes.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_note_record& note = notes.notes[index];
    const std::string rel_path_utf8 = ActiveCodePageToUtf8(note.rel_path);
    json += "{\"rel_path\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
    json += "\"title\":\"" + JsonEscape(note.title) + "\",";
    json += "\"file_size\":" + std::to_string(note.file_size) + ",";
    json += "\"mtime_ns\":" + std::to_string(note.mtime_ns) + ",";
    json += "\"content_revision\":\"" + JsonEscape(note.content_revision) + "\"}";
  }
  json += "],\"count\":" + std::to_string(notes.count) + "}";
  return json;
}

inline void AppendAiEmbeddingRefreshJobJson(
    std::string& json,
    const kernel_ai_embedding_refresh_job& job) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(job.rel_path);
  const std::string absolute_path_utf8 = ActiveCodePageToUtf8(job.absolute_path);
  const std::string content =
      job.content == nullptr ? std::string() : std::string(job.content, job.content_size);
  json += "{\"relPath\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"title\":\"" + JsonEscape(job.title) + "\",";
  json += "\"absolutePath\":\"" + JsonEscape(absolute_path_utf8.c_str()) + "\",";
  json += "\"createdAt\":" + std::to_string(job.created_at) + ",";
  json += "\"updatedAt\":" + std::to_string(job.updated_at) + ",";
  json += "\"content\":\"" + JsonEscape(content.c_str()) + "\"}";
}

inline void AppendFileTreeNoteJson(std::string& json, const kernel_file_tree_note& note) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(note.rel_path);
  const std::string name_utf8 = ActiveCodePageToUtf8(note.name);
  json += "{\"relPath\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"name\":\"" + JsonEscape(name_utf8.c_str()) + "\",";
  json += "\"extension\":\"" + JsonEscape(note.extension) + "\",";
  json += "\"mtimeNs\":" + std::to_string(note.mtime_ns) + "}";
}

inline void AppendFileTreeNodeJson(std::string& json, const kernel_file_tree_node& node) {
  const std::string name_utf8 = ActiveCodePageToUtf8(node.name);
  const std::string full_name_utf8 = ActiveCodePageToUtf8(node.full_name);
  const std::string relative_path_utf8 = ActiveCodePageToUtf8(node.relative_path);

  json += "{\"name\":\"" + JsonEscape(name_utf8.c_str()) + "\",";
  json += "\"fullName\":\"" + JsonEscape(full_name_utf8.c_str()) + "\",";
  json += "\"relativePath\":\"" + JsonEscape(relative_path_utf8.c_str()) + "\",";
  json += "\"isFolder\":";
  json += node.is_folder != 0 ? "true" : "false";
  json += ",\"note\":";
  if (node.has_note != 0) {
    AppendFileTreeNoteJson(json, node.note);
  } else {
    json += "null";
  }
  json += ",\"children\":[";
  for (size_t index = 0; index < node.child_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendFileTreeNodeJson(json, node.children[index]);
  }
  json += "],\"fileCount\":" + std::to_string(node.file_count) + "}";
}

inline void AppendTagTreeNodeJson(std::string& json, const kernel_tag_tree_node& node) {
  const std::string name_utf8 = ActiveCodePageToUtf8(node.name);
  const std::string full_path_utf8 = ActiveCodePageToUtf8(node.full_path);

  json += "{\"name\":\"" + JsonEscape(name_utf8.c_str()) + "\",";
  json += "\"fullPath\":\"" + JsonEscape(full_path_utf8.c_str()) + "\",";
  json += "\"count\":" + std::to_string(node.count) + ",";
  json += "\"children\":[";
  for (size_t index = 0; index < node.child_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendTagTreeNodeJson(json, node.children[index]);
  }
  json += "]}";
}

inline const char* ChemSpectrumFormatToken(const kernel_chem_spectrum_format value) {
  switch (value) {
    case KERNEL_CHEM_SPECTRUM_FORMAT_JCAMP_DX:
      return "jcamp_dx";
    case KERNEL_CHEM_SPECTRUM_FORMAT_SPECTRUM_CSV_V1:
      return "spectrum_csv_v1";
    case KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN:
      return "unknown";
  }
  return "unknown";
}

inline const char* AttachmentKindToken(const kernel_attachment_kind value) {
  switch (value) {
    case KERNEL_ATTACHMENT_KIND_GENERIC_FILE:
      return "generic_file";
    case KERNEL_ATTACHMENT_KIND_IMAGE_LIKE:
      return "image_like";
    case KERNEL_ATTACHMENT_KIND_PDF_LIKE:
      return "pdf_like";
    case KERNEL_ATTACHMENT_KIND_CHEM_LIKE:
      return "chem_like";
    case KERNEL_ATTACHMENT_KIND_UNKNOWN:
      return "unknown";
  }
  return "unknown";
}

inline const char* AttachmentPresenceToken(const kernel_attachment_presence value) {
  switch (value) {
    case KERNEL_ATTACHMENT_PRESENCE_PRESENT:
      return "present";
    case KERNEL_ATTACHMENT_PRESENCE_MISSING:
      return "missing";
  }
  return "missing";
}

inline const char* DomainObjectStateToken(const kernel_domain_object_state value) {
  switch (value) {
    case KERNEL_DOMAIN_OBJECT_PRESENT:
      return "present";
    case KERNEL_DOMAIN_OBJECT_MISSING:
      return "missing";
    case KERNEL_DOMAIN_OBJECT_UNRESOLVED:
      return "unresolved";
    case KERNEL_DOMAIN_OBJECT_UNSUPPORTED:
      return "unsupported";
  }
  return "unresolved";
}

inline const char* ChemSpectrumSelectorKindToken(const kernel_chem_spectrum_selector_kind value) {
  switch (value) {
    case KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM:
      return "whole_spectrum";
    case KERNEL_CHEM_SPECTRUM_SELECTOR_X_RANGE:
      return "x_range";
  }
  return "whole_spectrum";
}

inline const char* DomainRefStateToken(const kernel_domain_ref_state value) {
  switch (value) {
    case KERNEL_DOMAIN_REF_RESOLVED:
      return "resolved";
    case KERNEL_DOMAIN_REF_MISSING:
      return "missing";
    case KERNEL_DOMAIN_REF_STALE:
      return "stale";
    case KERNEL_DOMAIN_REF_UNRESOLVED:
      return "unresolved";
    case KERNEL_DOMAIN_REF_UNSUPPORTED:
      return "unsupported";
  }
  return "unresolved";
}

inline void AppendChemSpectrumJson(std::string& json, const kernel_chem_spectrum_record& spectrum) {
  const std::string attachment_rel_path_utf8 =
      ActiveCodePageToUtf8(spectrum.attachment_rel_path);
  json += "{\"attachmentRelPath\":\"" + JsonEscape(attachment_rel_path_utf8.c_str()) + "\",";
  json += "\"domainObjectKey\":\"" + JsonEscape(spectrum.domain_object_key) + "\",";
  json += "\"subtypeRevision\":" + std::to_string(spectrum.subtype_revision) + ",";
  json += "\"sourceFormat\":\"" + std::string(ChemSpectrumFormatToken(spectrum.source_format)) + "\",";
  json += "\"coarseKind\":\"" + std::string(AttachmentKindToken(spectrum.coarse_kind)) + "\",";
  json += "\"presence\":\"" + std::string(AttachmentPresenceToken(spectrum.presence)) + "\",";
  json += "\"state\":\"" + std::string(DomainObjectStateToken(spectrum.state)) + "\",";
  json += "\"flags\":" + std::to_string(spectrum.flags) + "}";
}

inline void AppendChemSpectrumSourceRefJson(
    std::string& json,
    const kernel_chem_spectrum_source_ref& ref) {
  const std::string attachment_rel_path_utf8 = ActiveCodePageToUtf8(ref.attachment_rel_path);
  json += "{\"attachmentRelPath\":\"" + JsonEscape(attachment_rel_path_utf8.c_str()) + "\",";
  json += "\"domainObjectKey\":\"" + JsonEscape(ref.domain_object_key) + "\",";
  json += "\"selectorKind\":\"" +
          std::string(ChemSpectrumSelectorKindToken(ref.selector_kind)) + "\",";
  json += "\"selectorSerialized\":\"" + JsonEscape(ref.selector_serialized) + "\",";
  json += "\"previewText\":\"" + JsonEscape(ref.preview_text) + "\",";
  json += "\"targetBasisRevision\":\"" + JsonEscape(ref.target_basis_revision) + "\",";
  json += "\"state\":\"" + std::string(DomainRefStateToken(ref.state)) + "\",";
  json += "\"flags\":" + std::to_string(ref.flags) + "}";
}

inline void AppendChemSpectrumReferrerJson(
    std::string& json,
    const kernel_chem_spectrum_referrer& referrer) {
  const std::string note_rel_path_utf8 = ActiveCodePageToUtf8(referrer.note_rel_path);
  const std::string attachment_rel_path_utf8 = ActiveCodePageToUtf8(referrer.attachment_rel_path);
  json += "{\"noteRelPath\":\"" + JsonEscape(note_rel_path_utf8.c_str()) + "\",";
  json += "\"noteTitle\":\"" + JsonEscape(referrer.note_title) + "\",";
  json += "\"attachmentRelPath\":\"" + JsonEscape(attachment_rel_path_utf8.c_str()) + "\",";
  json += "\"domainObjectKey\":\"" + JsonEscape(referrer.domain_object_key) + "\",";
  json += "\"selectorKind\":\"" +
          std::string(ChemSpectrumSelectorKindToken(referrer.selector_kind)) + "\",";
  json += "\"selectorSerialized\":\"" + JsonEscape(referrer.selector_serialized) + "\",";
  json += "\"previewText\":\"" + JsonEscape(referrer.preview_text) + "\",";
  json += "\"targetBasisRevision\":\"" + JsonEscape(referrer.target_basis_revision) + "\",";
  json += "\"state\":\"" + std::string(DomainRefStateToken(referrer.state)) + "\",";
  json += "\"flags\":" + std::to_string(referrer.flags) + "}";
}

inline const char* SpectroscopyErrorToken(const kernel_spectroscopy_parse_error value) {
  switch (value) {
    case KERNEL_SPECTROSCOPY_PARSE_ERROR_NONE:
      return "none";
    case KERNEL_SPECTROSCOPY_PARSE_ERROR_UNSUPPORTED_EXTENSION:
      return "unsupported_extension";
    case KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_NUMERIC_ROWS:
      return "csv_no_numeric_rows";
    case KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_TOO_FEW_COLUMNS:
      return "csv_too_few_columns";
    case KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_VALID_POINTS:
      return "csv_no_valid_points";
    case KERNEL_SPECTROSCOPY_PARSE_ERROR_JDX_NO_POINTS:
      return "jdx_no_points";
  }
  return "unknown";
}

inline const char* MolecularPreviewErrorToken(const kernel_molecular_preview_error value) {
  switch (value) {
    case KERNEL_MOLECULAR_PREVIEW_ERROR_NONE:
      return "none";
    case KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION:
      return "unsupported_extension";
  }
  return "unknown";
}

inline const char* SymmetryParseErrorToken(const kernel_symmetry_parse_error value) {
  switch (value) {
    case KERNEL_SYMMETRY_PARSE_ERROR_NONE:
      return "parse_none";
    case KERNEL_SYMMETRY_PARSE_ERROR_UNSUPPORTED_FORMAT:
      return "parse_unsupported_format";
    case KERNEL_SYMMETRY_PARSE_ERROR_XYZ_EMPTY:
      return "parse_xyz_empty";
    case KERNEL_SYMMETRY_PARSE_ERROR_XYZ_INCOMPLETE:
      return "parse_xyz_incomplete";
    case KERNEL_SYMMETRY_PARSE_ERROR_XYZ_COORDINATE:
      return "parse_xyz_coordinate";
    case KERNEL_SYMMETRY_PARSE_ERROR_PDB_COORDINATE:
      return "parse_pdb_coordinate";
    case KERNEL_SYMMETRY_PARSE_ERROR_CIF_MISSING_CELL:
      return "parse_cif_missing_cell";
    case KERNEL_SYMMETRY_PARSE_ERROR_CIF_INVALID_CELL:
      return "parse_cif_invalid_cell";
  }
  return "parse_unknown";
}

inline std::string SymmetryCalculationErrorToken(
    const kernel_symmetry_calculation_result& result) {
  switch (result.error) {
    case KERNEL_SYMMETRY_CALC_ERROR_NONE:
      return "none";
    case KERNEL_SYMMETRY_CALC_ERROR_PARSE:
      return SymmetryParseErrorToken(result.parse_error);
    case KERNEL_SYMMETRY_CALC_ERROR_NO_ATOMS:
      return "no_atoms";
    case KERNEL_SYMMETRY_CALC_ERROR_TOO_MANY_ATOMS:
      return "too_many_atoms:" + std::to_string(result.atom_count);
    case KERNEL_SYMMETRY_CALC_ERROR_INTERNAL:
      return "internal";
  }
  return "unknown";
}

inline const char* CrystalParseErrorToken(const kernel_crystal_parse_error value) {
  switch (value) {
    case KERNEL_CRYSTAL_PARSE_ERROR_NONE:
      return "parse_none";
    case KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL:
      return "parse_missing_cell";
    case KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS:
      return "parse_missing_atoms";
  }
  return "parse_unknown";
}

inline std::string CrystalSupercellErrorToken(
    const kernel_crystal_supercell_error value,
    const uint64_t estimated_count) {
  switch (value) {
    case KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE:
      return "supercell_none";
    case KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL:
      return "supercell_gamma_too_small";
    case KERNEL_CRYSTAL_SUPERCELL_ERROR_INVALID_BASIS:
      return "supercell_invalid_basis";
    case KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS:
      return "supercell_too_many_atoms:" + std::to_string(estimated_count);
  }
  return "supercell_unknown";
}

inline const char* CrystalMillerErrorToken(const kernel_crystal_miller_error value) {
  switch (value) {
    case KERNEL_CRYSTAL_MILLER_ERROR_NONE:
      return "miller_none";
    case KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX:
      return "miller_zero_index";
    case KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL:
      return "miller_gamma_too_small";
    case KERNEL_CRYSTAL_MILLER_ERROR_INVALID_BASIS:
      return "miller_invalid_basis";
    case KERNEL_CRYSTAL_MILLER_ERROR_ZERO_VOLUME:
      return "miller_zero_volume";
    case KERNEL_CRYSTAL_MILLER_ERROR_ZERO_NORMAL:
      return "miller_zero_normal";
  }
  return "miller_unknown";
}

inline std::string CrystalLatticeErrorToken(const kernel_lattice_result& result) {
  if (result.parse_error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    return CrystalParseErrorToken(result.parse_error);
  }
  if (result.supercell_error != KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE) {
    return CrystalSupercellErrorToken(result.supercell_error, result.estimated_count);
  }
  return "lattice_unknown";
}

inline std::string CrystalCifMillerErrorToken(const kernel_cif_miller_plane_result& result) {
  if (result.parse_error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    return CrystalParseErrorToken(result.parse_error);
  }
  return CrystalMillerErrorToken(result.plane.error);
}

inline bool ValidateRetroTreeJsonInput(const kernel_retro_tree& tree, std::string& error) {
  if (tree.pathway_count == 0) {
    error = "empty_tree";
    return false;
  }
  if (tree.pathways == nullptr) {
    error = "invalid_payload";
    return false;
  }

  for (size_t pathway_index = 0; pathway_index < tree.pathway_count; ++pathway_index) {
    const kernel_retro_pathway& pathway = tree.pathways[pathway_index];
    if (pathway.target_id == nullptr ||
        pathway.reaction_name == nullptr ||
        pathway.conditions == nullptr) {
      error = "invalid_payload";
      return false;
    }
    if (pathway.precursor_count > 0 && pathway.precursors == nullptr) {
      error = "invalid_payload";
      return false;
    }
    for (size_t precursor_index = 0; precursor_index < pathway.precursor_count; ++precursor_index) {
      const kernel_retro_precursor& precursor = pathway.precursors[precursor_index];
      if (precursor.id == nullptr ||
          precursor.smiles == nullptr ||
          precursor.role == nullptr) {
        error = "invalid_payload";
        return false;
      }
    }
  }
  return true;
}

inline bool ValidateKineticsJsonInput(
    const kernel_polymerization_kinetics_result& result,
    std::string& error) {
  if (result.count == 0) {
    error = "empty_result";
    return false;
  }
  if (result.time == nullptr ||
      result.conversion == nullptr ||
      result.mn == nullptr ||
      result.pdi == nullptr) {
    error = "invalid_payload";
    return false;
  }
  return true;
}

inline bool ValidateInkSmoothingJsonInput(
    const kernel_ink_smoothing_result& result,
    std::string& error) {
  if (result.count > 0 && result.strokes == nullptr) {
    error = "invalid_payload";
    return false;
  }
  for (size_t stroke_index = 0; stroke_index < result.count; ++stroke_index) {
    const kernel_ink_stroke& stroke = result.strokes[stroke_index];
    if (stroke.point_count > 0 && stroke.points == nullptr) {
      error = "invalid_payload";
      return false;
    }
  }
  return true;
}

inline bool ValidateTruthDiffJsonInput(
    const kernel_truth_diff_result& result,
    std::string& error) {
  if (result.count > 0 && result.awards == nullptr) {
    error = "invalid_payload";
    return false;
  }
  for (size_t index = 0; index < result.count; ++index) {
    const kernel_truth_award& award = result.awards[index];
    if (award.attr == nullptr) {
      error = "invalid_payload";
      return false;
    }
    const char* reason_key = nullptr;
    if (kernel_get_truth_award_reason_key(award.reason, &reason_key).code != KERNEL_OK ||
        reason_key == nullptr) {
      error = "invalid_payload";
      return false;
    }
  }
  return true;
}

inline void AppendDoubleArrayJson(std::string& json, const double* values, const size_t count) {
  json += "[";
  for (size_t index = 0; index < count; ++index) {
    if (index != 0) {
      json += ",";
    }
    json += std::to_string(values[index]);
  }
  json += "]";
}

inline void AppendSpectroscopyJson(std::string& json, const kernel_spectroscopy_data& data) {
  json += "{\"x\":";
  AppendDoubleArrayJson(json, data.x, data.x_count);
  json += ",\"series\":[";
  for (size_t index = 0; index < data.series_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_spectrum_series& series = data.series[index];
    json += "{\"y\":";
    AppendDoubleArrayJson(json, series.y, series.count);
    json += ",\"label\":\"" + JsonEscape(series.label) + "\"}";
  }
  json += "],\"x_label\":\"" + JsonEscape(data.x_label) + "\",";
  json += "\"title\":\"" + JsonEscape(data.title) + "\",";
  json += "\"is_nmr\":";
  json += data.is_nmr != 0 ? "true" : "false";
  json += "}";
}

inline void AppendMolecularPreviewJson(std::string& json, const kernel_molecular_preview& preview) {
  json += "{\"preview_data\":\"" + JsonEscape(preview.preview_data) + "\",";
  json += "\"atom_count\":" + std::to_string(preview.atom_count) + ",";
  json += "\"preview_atom_count\":" + std::to_string(preview.preview_atom_count) + ",";
  json += "\"truncated\":";
  json += preview.truncated != 0 ? "true" : "false";
  json += "}";
}

inline void AppendTruthDiffJson(std::string& json, const kernel_truth_diff_result& result) {
  json += "{\"awards\":[";
  for (size_t index = 0; index < result.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_truth_award& award = result.awards[index];
    const char* reason_key = nullptr;
    kernel_get_truth_award_reason_key(award.reason, &reason_key);
    json += "{\"attr\":\"" + JsonEscape(award.attr) + "\",";
    json += "\"amount\":" + std::to_string(award.amount) + ",";
    json += "\"reasonKey\":\"" + JsonEscape(reason_key) + "\",";
    json += "\"detail\":\"" + JsonEscape(award.detail) + "\"}";
  }
  json += "]}";
}

inline void AppendTruthAttributesJson(std::string& json, const kernel_truth_attribute_values& values) {
  json += "{\"science\":" + std::to_string(values.science);
  json += ",\"engineering\":" + std::to_string(values.engineering);
  json += ",\"creation\":" + std::to_string(values.creation);
  json += ",\"finance\":" + std::to_string(values.finance) + "}";
}

inline void AppendTruthStateJson(std::string& json, const kernel_truth_state_snapshot& state) {
  json += "{\"level\":" + std::to_string(state.level);
  json += ",\"totalExp\":" + std::to_string(state.total_exp);
  json += ",\"nextLevelExp\":" + std::to_string(state.next_level_exp);
  json += ",\"attributes\":";
  AppendTruthAttributesJson(json, state.attributes);
  json += ",\"attributeExp\":";
  AppendTruthAttributesJson(json, state.attribute_exp);
  json += "}";
}

inline void AppendHeatmapGridJson(std::string& json, const kernel_heatmap_grid& grid) {
  json += "{\"cells\":[";
  for (size_t index = 0; index < grid.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_heatmap_cell& cell = grid.cells[index];
    const std::string date_utf8 = ActiveCodePageToUtf8(cell.date);
    json += "{\"date\":\"" + JsonEscape(date_utf8.c_str()) + "\",";
    json += "\"secs\":" + std::to_string(cell.secs) + ",";
    json += "\"col\":" + std::to_string(cell.col) + ",";
    json += "\"row\":" + std::to_string(cell.row) + "}";
  }
  json += "],\"maxSecs\":" + std::to_string(grid.max_secs) + "}";
}

inline void AppendRetroPrecursorJson(std::string& json, const kernel_retro_precursor& precursor) {
  json += "{\"id\":\"" + JsonEscape(precursor.id) + "\",";
  json += "\"smiles\":\"" + JsonEscape(precursor.smiles) + "\",";
  json += "\"role\":\"" + JsonEscape(precursor.role) + "\"}";
}

inline void AppendRetroPathwayJson(std::string& json, const kernel_retro_pathway& pathway) {
  json += "{\"target_id\":\"" + JsonEscape(pathway.target_id) + "\",";
  json += "\"precursors\":[";
  for (size_t index = 0; index < pathway.precursor_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendRetroPrecursorJson(json, pathway.precursors[index]);
  }
  json += "],\"reaction_name\":\"" + JsonEscape(pathway.reaction_name) + "\",";
  json += "\"conditions\":\"" + JsonEscape(pathway.conditions) + "\"}";
}

inline void AppendRetroTreeJson(std::string& json, const kernel_retro_tree& tree) {
  json += "{\"pathways\":[";
  for (size_t index = 0; index < tree.pathway_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendRetroPathwayJson(json, tree.pathways[index]);
  }
  json += "]}";
}

inline void AppendKineticsJson(
    std::string& json,
    const kernel_polymerization_kinetics_result& result) {
  json += "{\"time\":";
  AppendDoubleArrayJson(json, result.time, result.count);
  json += ",\"conversion\":";
  AppendDoubleArrayJson(json, result.conversion, result.count);
  json += ",\"mn\":";
  AppendDoubleArrayJson(json, result.mn, result.count);
  json += ",\"pdi\":";
  AppendDoubleArrayJson(json, result.pdi, result.count);
  json += "}";
}

inline void AppendStoichiometryJson(
    std::string& json,
    const std::vector<kernel_stoichiometry_row_output>& rows) {
  json += "{\"rows\":[";
  for (size_t index = 0; index < rows.size(); ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_stoichiometry_row_output& row = rows[index];
    json += "{\"mw\":" + std::to_string(row.mw) + ",";
    json += "\"eq\":" + std::to_string(row.eq) + ",";
    json += "\"moles\":" + std::to_string(row.moles) + ",";
    json += "\"mass\":" + std::to_string(row.mass) + ",";
    json += "\"volume\":" + std::to_string(row.volume) + ",";
    json += "\"density\":" + std::to_string(row.density) + ",";
    json += "\"hasDensity\":";
    json += row.has_density != 0 ? "true" : "false";
    json += ",\"isReference\":";
    json += row.is_reference != 0 ? "true" : "false";
    json += "}";
  }
  json += "]}";
}

inline void AppendFloatJson(std::string& json, const float value) {
  json += std::to_string(value);
}

inline void AppendInkPointJson(std::string& json, const kernel_ink_point& point) {
  json += "{\"x\":";
  AppendFloatJson(json, point.x);
  json += ",\"y\":";
  AppendFloatJson(json, point.y);
  json += ",\"pressure\":";
  AppendFloatJson(json, point.pressure);
  json += "}";
}

inline void AppendInkSmoothingJson(std::string& json, const kernel_ink_smoothing_result& result) {
  json += "[";
  for (size_t stroke_index = 0; stroke_index < result.count; ++stroke_index) {
    if (stroke_index != 0) {
      json += ",";
    }
    const kernel_ink_stroke& stroke = result.strokes[stroke_index];
    json += "{\"points\":[";
    for (size_t point_index = 0; point_index < stroke.point_count; ++point_index) {
      if (point_index != 0) {
        json += ",";
      }
      AppendInkPointJson(json, stroke.points[point_index]);
    }
    json += "],\"strokeWidth\":";
    AppendFloatJson(json, stroke.stroke_width);
    json += "}";
  }
  json += "]";
}

inline void AppendVec3Json(std::string& json, const double values[3]) {
  json += "{\"x\":" + std::to_string(values[0]) + ",";
  json += "\"y\":" + std::to_string(values[1]) + ",";
  json += "\"z\":" + std::to_string(values[2]) + "}";
}

inline void AppendSymmetryAxisJson(std::string& json, const kernel_symmetry_render_axis& axis) {
  json += "{\"vector\":";
  AppendVec3Json(json, axis.vector);
  json += ",\"center\":";
  AppendVec3Json(json, axis.center);
  json += ",\"order\":" + std::to_string(axis.order) + ",";
  json += "\"start\":";
  AppendVec3Json(json, axis.start);
  json += ",\"end\":";
  AppendVec3Json(json, axis.end);
  json += "}";
}

inline void AppendSymmetryPlaneJson(std::string& json, const kernel_symmetry_render_plane& plane) {
  json += "{\"normal\":";
  AppendVec3Json(json, plane.normal);
  json += ",\"center\":";
  AppendVec3Json(json, plane.center);
  json += ",\"vertices\":[";
  for (size_t index = 0; index < 4; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendVec3Json(json, plane.vertices[index]);
  }
  json += "]}";
}

inline void AppendSymmetryJson(std::string& json, const kernel_symmetry_calculation_result& result) {
  json += "{\"pointGroup\":\"" + JsonEscape(result.point_group) + "\",";
  json += "\"planes\":[";
  for (size_t index = 0; index < result.plane_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendSymmetryPlaneJson(json, result.planes[index]);
  }
  json += "],\"axes\":[";
  for (size_t index = 0; index < result.axis_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendSymmetryAxisJson(json, result.axes[index]);
  }
  json += "],\"hasInversion\":";
  json += result.has_inversion != 0 ? "true" : "false";
  json += ",\"atomCount\":" + std::to_string(result.atom_count) + "}";
}

inline void AppendDoubleFixedArrayJson(std::string& json, const double* values, const size_t count) {
  json += "[";
  for (size_t index = 0; index < count; ++index) {
    if (index != 0) {
      json += ",";
    }
    json += std::to_string(values[index]);
  }
  json += "]";
}

inline void AppendCrystalMatrixJson(std::string& json, const double values[3][3]) {
  json += "[";
  for (size_t row = 0; row < 3; ++row) {
    if (row != 0) {
      json += ",";
    }
    AppendDoubleFixedArrayJson(json, values[row], 3);
  }
  json += "]";
}

inline void AppendCrystalUnitCellJson(std::string& json, const kernel_unit_cell_box& unit_cell) {
  json += "{\"a\":" + std::to_string(unit_cell.a) + ",";
  json += "\"b\":" + std::to_string(unit_cell.b) + ",";
  json += "\"c\":" + std::to_string(unit_cell.c) + ",";
  json += "\"alpha\":" + std::to_string(unit_cell.alpha_deg) + ",";
  json += "\"beta\":" + std::to_string(unit_cell.beta_deg) + ",";
  json += "\"gamma\":" + std::to_string(unit_cell.gamma_deg) + ",";
  json += "\"origin\":";
  AppendDoubleFixedArrayJson(json, unit_cell.origin, 3);
  json += ",\"vectors\":";
  AppendCrystalMatrixJson(json, unit_cell.vectors);
  json += "}";
}

inline void AppendCrystalAtomJson(std::string& json, const kernel_atom_node& atom) {
  json += "{\"element\":\"" + JsonEscape(atom.element) + "\",";
  json += "\"cartesianCoords\":";
  AppendDoubleFixedArrayJson(json, atom.cartesian_coords, 3);
  json += "}";
}

inline void AppendCrystalLatticeJson(std::string& json, const kernel_lattice_result& result) {
  json += "{\"unitCell\":";
  AppendCrystalUnitCellJson(json, result.unit_cell);
  json += ",\"atoms\":[";
  for (size_t index = 0; index < result.atom_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendCrystalAtomJson(json, result.atoms[index]);
  }
  json += "]}";
}

inline void AppendCrystalMillerPlaneJson(std::string& json, const kernel_miller_plane_result& plane) {
  json += "{\"normal\":";
  AppendDoubleFixedArrayJson(json, plane.normal, 3);
  json += ",\"center\":";
  AppendDoubleFixedArrayJson(json, plane.center, 3);
  json += ",\"d\":" + std::to_string(plane.d) + ",";
  json += "\"vertices\":[";
  for (size_t index = 0; index < 4; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendDoubleFixedArrayJson(json, plane.vertices[index], 3);
  }
  json += "]}";
}

inline int32_t KernelDefaultLimit(
    kernel_status (*getter)(size_t*),
    const char* operation,
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = getter(&limit);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, operation, out_error);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

inline int32_t CopyKernelOwnedText(
    kernel_owned_buffer& buffer,
    char** out_text,
    char** out_error) {
  const std::string text = buffer.data == nullptr || buffer.size == 0
                               ? std::string()
                               : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  *out_text = CopyString(text);
  if (*out_text == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}


inline int32_t KernelDefaultFloat(
    kernel_status (*getter)(float*),
    const char* operation,
    float* out_value,
    char** out_error) {
  if (out_value == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  float value = 0.0F;
  const kernel_status status = getter(&value);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, operation, out_error);
  }

  *out_value = value;
  return static_cast<int32_t>(KERNEL_OK);
}


}  // namespace sealed_kernel_bridge_internal
