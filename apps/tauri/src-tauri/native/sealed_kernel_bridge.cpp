#include "sealed_kernel_bridge.h"

#include "kernel/c_api.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct sealed_kernel_bridge_session {
  kernel_handle* handle = nullptr;
};

namespace {

char* CopyString(const std::string& value) {
  auto* out = static_cast<char*>(std::malloc(value.size() + 1));
  if (out == nullptr) {
    return nullptr;
  }
  std::memcpy(out, value.c_str(), value.size() + 1);
  return out;
}

void SetError(char** out_error, const std::string& message) {
  if (out_error == nullptr) {
    return;
  }
  *out_error = CopyString(message);
}

const char* KernelErrorCodeName(const kernel_error_code code) {
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

std::string Utf8ToActiveCodePage(const char* value) {
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

std::string ActiveCodePageToUtf8(const char* value) {
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

int32_t ReturnKernelError(
    const kernel_status status,
    const char* operation,
    char** out_error) {
  const char* code_name = KernelErrorCodeName(status.code);
  SetError(out_error, std::string(operation) + " failed (" + code_name + ").");
  return static_cast<int32_t>(status.code);
}

std::string JsonEscape(const char* value) {
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

std::string PathListToJson(const kernel_path_list& paths) {
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

void AppendNoteHitJson(
    std::string& json,
    const char* rel_path,
    const char* title,
    uint64_t mtime_ns = 0) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(rel_path);
  json += "{\"rel_path\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"title\":\"" + JsonEscape(title) + "\",";
  json += "\"mtime_ns\":" + std::to_string(mtime_ns) + "}";
}

void AppendFileTreeNoteJson(std::string& json, const kernel_file_tree_note& note) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(note.rel_path);
  const std::string name_utf8 = ActiveCodePageToUtf8(note.name);
  json += "{\"relPath\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"name\":\"" + JsonEscape(name_utf8.c_str()) + "\",";
  json += "\"extension\":\"" + JsonEscape(note.extension) + "\",";
  json += "\"mtimeNs\":" + std::to_string(note.mtime_ns) + "}";
}

void AppendFileTreeNodeJson(std::string& json, const kernel_file_tree_node& node) {
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

const char* ChemSpectrumFormatToken(const kernel_chem_spectrum_format value) {
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

const char* AttachmentKindToken(const kernel_attachment_kind value) {
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

const char* AttachmentPresenceToken(const kernel_attachment_presence value) {
  switch (value) {
    case KERNEL_ATTACHMENT_PRESENCE_PRESENT:
      return "present";
    case KERNEL_ATTACHMENT_PRESENCE_MISSING:
      return "missing";
  }
  return "missing";
}

const char* DomainObjectStateToken(const kernel_domain_object_state value) {
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

const char* ChemSpectrumSelectorKindToken(const kernel_chem_spectrum_selector_kind value) {
  switch (value) {
    case KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM:
      return "whole_spectrum";
    case KERNEL_CHEM_SPECTRUM_SELECTOR_X_RANGE:
      return "x_range";
  }
  return "whole_spectrum";
}

const char* DomainRefStateToken(const kernel_domain_ref_state value) {
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

void AppendChemSpectrumJson(std::string& json, const kernel_chem_spectrum_record& spectrum) {
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

void AppendChemSpectrumSourceRefJson(
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

void AppendChemSpectrumReferrerJson(
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

const char* SpectroscopyErrorToken(const kernel_spectroscopy_parse_error value) {
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

const char* MolecularPreviewErrorToken(const kernel_molecular_preview_error value) {
  switch (value) {
    case KERNEL_MOLECULAR_PREVIEW_ERROR_NONE:
      return "none";
    case KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION:
      return "unsupported_extension";
  }
  return "unknown";
}

const char* SymmetryParseErrorToken(const kernel_symmetry_parse_error value) {
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

std::string SymmetryCalculationErrorToken(
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

const char* CrystalParseErrorToken(const kernel_crystal_parse_error value) {
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

std::string CrystalSupercellErrorToken(
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

const char* CrystalMillerErrorToken(const kernel_crystal_miller_error value) {
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

std::string CrystalLatticeErrorToken(const kernel_lattice_result& result) {
  if (result.parse_error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    return CrystalParseErrorToken(result.parse_error);
  }
  if (result.supercell_error != KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE) {
    return CrystalSupercellErrorToken(result.supercell_error, result.estimated_count);
  }
  return "lattice_unknown";
}

std::string CrystalCifMillerErrorToken(const kernel_cif_miller_plane_result& result) {
  if (result.parse_error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    return CrystalParseErrorToken(result.parse_error);
  }
  return CrystalMillerErrorToken(result.plane.error);
}

bool ValidateRetroTreeJsonInput(const kernel_retro_tree& tree, std::string& error) {
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

bool ValidateKineticsJsonInput(
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

bool ValidateInkSmoothingJsonInput(
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

bool ValidateTruthDiffJsonInput(
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
  }
  return true;
}

void AppendDoubleArrayJson(std::string& json, const double* values, const size_t count) {
  json += "[";
  for (size_t index = 0; index < count; ++index) {
    if (index != 0) {
      json += ",";
    }
    json += std::to_string(values[index]);
  }
  json += "]";
}

void AppendSpectroscopyJson(std::string& json, const kernel_spectroscopy_data& data) {
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

void AppendMolecularPreviewJson(std::string& json, const kernel_molecular_preview& preview) {
  json += "{\"preview_data\":\"" + JsonEscape(preview.preview_data) + "\",";
  json += "\"atom_count\":" + std::to_string(preview.atom_count) + ",";
  json += "\"preview_atom_count\":" + std::to_string(preview.preview_atom_count) + ",";
  json += "\"truncated\":";
  json += preview.truncated != 0 ? "true" : "false";
  json += "}";
}

void AppendTruthDiffJson(std::string& json, const kernel_truth_diff_result& result) {
  json += "{\"awards\":[";
  for (size_t index = 0; index < result.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_truth_award& award = result.awards[index];
    json += "{\"attr\":\"" + JsonEscape(award.attr) + "\",";
    json += "\"amount\":" + std::to_string(award.amount) + ",";
    json += "\"reason\":" + std::to_string(static_cast<int32_t>(award.reason)) + ",";
    json += "\"detail\":\"" + JsonEscape(award.detail) + "\"}";
  }
  json += "]}";
}

void AppendTruthAttributesJson(std::string& json, const kernel_truth_attribute_values& values) {
  json += "{\"science\":" + std::to_string(values.science);
  json += ",\"engineering\":" + std::to_string(values.engineering);
  json += ",\"creation\":" + std::to_string(values.creation);
  json += ",\"finance\":" + std::to_string(values.finance) + "}";
}

void AppendTruthStateJson(std::string& json, const kernel_truth_state_snapshot& state) {
  json += "{\"level\":" + std::to_string(state.level);
  json += ",\"totalExp\":" + std::to_string(state.total_exp);
  json += ",\"nextLevelExp\":" + std::to_string(state.next_level_exp);
  json += ",\"attributes\":";
  AppendTruthAttributesJson(json, state.attributes);
  json += ",\"attributeExp\":";
  AppendTruthAttributesJson(json, state.attribute_exp);
  json += "}";
}

void AppendHeatmapGridJson(std::string& json, const kernel_heatmap_grid& grid) {
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

void AppendRetroPrecursorJson(std::string& json, const kernel_retro_precursor& precursor) {
  json += "{\"id\":\"" + JsonEscape(precursor.id) + "\",";
  json += "\"smiles\":\"" + JsonEscape(precursor.smiles) + "\",";
  json += "\"role\":\"" + JsonEscape(precursor.role) + "\"}";
}

void AppendRetroPathwayJson(std::string& json, const kernel_retro_pathway& pathway) {
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

void AppendRetroTreeJson(std::string& json, const kernel_retro_tree& tree) {
  json += "{\"pathways\":[";
  for (size_t index = 0; index < tree.pathway_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendRetroPathwayJson(json, tree.pathways[index]);
  }
  json += "]}";
}

void AppendKineticsJson(
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

void AppendStoichiometryJson(
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

void AppendFloatJson(std::string& json, const float value) {
  json += std::to_string(value);
}

void AppendInkPointJson(std::string& json, const kernel_ink_point& point) {
  json += "{\"x\":";
  AppendFloatJson(json, point.x);
  json += ",\"y\":";
  AppendFloatJson(json, point.y);
  json += ",\"pressure\":";
  AppendFloatJson(json, point.pressure);
  json += "}";
}

void AppendInkSmoothingJson(std::string& json, const kernel_ink_smoothing_result& result) {
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

void AppendVec3Json(std::string& json, const double values[3]) {
  json += "{\"x\":" + std::to_string(values[0]) + ",";
  json += "\"y\":" + std::to_string(values[1]) + ",";
  json += "\"z\":" + std::to_string(values[2]) + "}";
}

void AppendSymmetryAxisJson(std::string& json, const kernel_symmetry_render_axis& axis) {
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

void AppendSymmetryPlaneJson(std::string& json, const kernel_symmetry_render_plane& plane) {
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

void AppendSymmetryJson(std::string& json, const kernel_symmetry_calculation_result& result) {
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

void AppendDoubleFixedArrayJson(std::string& json, const double* values, const size_t count) {
  json += "[";
  for (size_t index = 0; index < count; ++index) {
    if (index != 0) {
      json += ",";
    }
    json += std::to_string(values[index]);
  }
  json += "]";
}

void AppendCrystalMatrixJson(std::string& json, const double values[3][3]) {
  json += "[";
  for (size_t row = 0; row < 3; ++row) {
    if (row != 0) {
      json += ",";
    }
    AppendDoubleFixedArrayJson(json, values[row], 3);
  }
  json += "]";
}

void AppendCrystalUnitCellJson(std::string& json, const kernel_unit_cell_box& unit_cell) {
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

void AppendCrystalAtomJson(std::string& json, const kernel_atom_node& atom) {
  json += "{\"element\":\"" + JsonEscape(atom.element) + "\",";
  json += "\"cartesianCoords\":";
  AppendDoubleFixedArrayJson(json, atom.cartesian_coords, 3);
  json += "}";
}

void AppendCrystalLatticeJson(std::string& json, const kernel_lattice_result& result) {
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

void AppendCrystalMillerPlaneJson(std::string& json, const kernel_miller_plane_result& plane) {
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

}  // namespace

char* sealed_kernel_bridge_info_json(void) {
  return CopyString(
      "{\"adapter\":\"sealed-kernel-cpp-bridge\","
      "\"kernel\":\"chem_kernel\","
      "\"link_mode\":\"static-lib\","
      "\"path_encoding\":\"utf8-to-active-code-page\"}");
}

void sealed_kernel_bridge_free_string(char* value) {
  std::free(value);
}

int32_t sealed_kernel_bridge_open_vault_utf8(
    const char* vault_path_utf8,
    sealed_kernel_bridge_session** out_session,
    char** out_error) {
  if (out_session == nullptr) {
    SetError(out_error, "sealed_kernel_bridge_open_vault_utf8 missing out_session.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_session = nullptr;

  const std::string vault_path = Utf8ToActiveCodePage(vault_path_utf8);
  if (vault_path.empty()) {
    SetError(out_error, "vault_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_handle* handle = nullptr;
  const kernel_status status = kernel_open_vault(vault_path.c_str(), &handle);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_open_vault", out_error);
  }

  auto* session = new sealed_kernel_bridge_session();
  session->handle = handle;
  *out_session = session;
  return static_cast<int32_t>(KERNEL_OK);
}

void sealed_kernel_bridge_close(sealed_kernel_bridge_session* session) {
  if (session == nullptr) {
    return;
  }

  if (session->handle != nullptr) {
    kernel_close(session->handle);
    session->handle = nullptr;
  }
  delete session;
}

int32_t sealed_kernel_bridge_get_state(
    sealed_kernel_bridge_session* session,
    sealed_kernel_bridge_state_snapshot* out_state,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr || out_state == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_state_snapshot snapshot{};
  const kernel_status status = kernel_get_state(session->handle, &snapshot);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_get_state", out_error);
  }

  out_state->session_state = static_cast<int32_t>(snapshot.session_state);
  out_state->index_state = static_cast<int32_t>(snapshot.index_state);
  out_state->indexed_note_count = snapshot.indexed_note_count;
  out_state->pending_recovery_ops = snapshot.pending_recovery_ops;
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_note_catalog_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_note_catalog_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_note_query_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_note_query_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_vault_scan_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_vault_scan_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t QueryNotesJson(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or note query arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_note_list notes{};
  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  const kernel_status status = ignored_roots.empty()
      ? kernel_query_notes(session->handle, static_cast<size_t>(limit), &notes)
      : kernel_query_notes_filtered(
            session->handle,
            static_cast<size_t>(limit),
            ignored_roots.c_str(),
            &notes);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(
        status,
        ignored_roots.empty() ? "kernel_query_notes" : "kernel_query_notes_filtered",
        out_error);
  }

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

  kernel_free_note_list(&notes);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note catalog JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_notes_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  return QueryNotesJson(session, limit, nullptr, out_json, out_error);
}

int32_t sealed_kernel_bridge_query_notes_filtered_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  return QueryNotesJson(session, limit, ignored_roots_utf8, out_json, out_error);
}

int32_t sealed_kernel_bridge_get_file_tree_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_file_tree_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_file_tree_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or file tree arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_file_tree tree{};
  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  const kernel_status status = kernel_query_file_tree_filtered(
      session->handle,
      static_cast<size_t>(limit),
      ignored_roots.empty() ? nullptr : ignored_roots.c_str(),
      &tree);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_file_tree_filtered", out_error);
  }

  std::string json = "{\"nodes\":[";
  for (size_t index = 0; index < tree.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendFileTreeNodeJson(json, tree.nodes[index]);
  }
  json += "],\"count\":" + std::to_string(tree.count) + "}";

  kernel_free_file_tree(&tree);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate file tree JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_filter_changed_markdown_paths_json(
    const char* changed_paths_lf_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "changed markdown path output pointer is invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  kernel_path_list paths{};
  const kernel_status status =
      kernel_filter_changed_markdown_paths(changed_paths.c_str(), &paths);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_filter_changed_markdown_paths", out_error);
  }

  const std::string json = PathListToJson(paths);
  kernel_free_path_list(&paths);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate changed markdown path JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_filter_supported_vault_paths_filtered_json(
    const char* changed_paths_lf_utf8,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "filtered supported vault path output pointer is invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  kernel_path_list paths{};
  const kernel_status status = kernel_filter_supported_vault_paths_filtered(
      changed_paths.c_str(),
      ignored_roots.c_str(),
      &paths);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_filter_supported_vault_paths_filtered", out_error);
  }

  const std::string json = PathListToJson(paths);
  kernel_free_path_list(&paths);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate filtered supported vault path JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_read_note_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  if (rel_path.empty()) {
    SetError(out_error, "rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  kernel_note_metadata metadata{};
  const kernel_status status =
      kernel_read_note(session->handle, rel_path.c_str(), &buffer, &metadata);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_read_note", out_error);
  }

  const std::string content(buffer.data == nullptr ? "" : std::string(buffer.data, buffer.size));
  std::string json = "{\"content\":\"" + JsonEscape(content.c_str()) + "\",";
  json += "\"metadata\":{\"file_size\":" + std::to_string(metadata.file_size) + ",";
  json += "\"mtime_ns\":" + std::to_string(metadata.mtime_ns) + ",";
  json += "\"content_revision\":\"" + JsonEscape(metadata.content_revision) + "\"}}";
  kernel_free_buffer(&buffer);

  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note read JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_write_note_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    const char* content_utf8,
    uint64_t content_size,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      content_utf8 == nullptr) {
    SetError(out_error, "sealed kernel session is not open or write arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  if (rel_path.empty()) {
    SetError(out_error, "rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_note_metadata current_metadata{};
  kernel_owned_buffer current_buffer{};
  const kernel_status read_status =
      kernel_read_note(session->handle, rel_path.c_str(), &current_buffer, &current_metadata);
  const bool current_exists = read_status.code == KERNEL_OK;
  if (current_exists) {
    kernel_free_buffer(&current_buffer);
  } else if (read_status.code != KERNEL_ERROR_NOT_FOUND) {
    return ReturnKernelError(read_status, "kernel_read_note", out_error);
  }

  kernel_note_metadata written_metadata{};
  kernel_write_disposition disposition{};
  const char* expected_revision = current_exists ? current_metadata.content_revision : nullptr;
  const kernel_status write_status = kernel_write_note(
      session->handle,
      rel_path.c_str(),
      content_utf8,
      static_cast<size_t>(content_size),
      expected_revision,
      &written_metadata,
      &disposition);
  if (write_status.code != KERNEL_OK) {
    return ReturnKernelError(write_status, "kernel_write_note", out_error);
  }

  std::string json = "{\"metadata\":{\"file_size\":" + std::to_string(written_metadata.file_size) + ",";
  json += "\"mtime_ns\":" + std::to_string(written_metadata.mtime_ns) + ",";
  json += "\"content_revision\":\"" + JsonEscape(written_metadata.content_revision) + "\"},";
  json += "\"disposition\":" + std::to_string(static_cast<int>(disposition)) + "}";

  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note write JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_search_notes_json(
    sealed_kernel_bridge_session* session,
    const char* query_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      query_utf8 == nullptr || query_utf8[0] == '\0' || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or query search arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_search_query request{};
  request.query = query_utf8;
  request.limit = static_cast<size_t>(limit);
  request.offset = 0;
  request.kind = KERNEL_SEARCH_KIND_NOTE;
  request.tag_filter = nullptr;
  request.path_prefix = nullptr;
  request.include_deleted = 0;
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel_search_page page{};
  const kernel_status status =
      kernel_query_search(session->handle, &request, &page);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_search", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < page.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendNoteHitJson(json, page.hits[index].rel_path, page.hits[index].title);
  }
  json += "],\"count\":" + std::to_string(page.count) + "}";

  kernel_free_search_page(&page);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate query search JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

static int32_t KernelDefaultLimit(
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

int32_t sealed_kernel_bridge_get_search_note_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_search_note_default_limit,
      "kernel_get_search_note_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_backlink_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_backlink_default_limit,
      "kernel_get_backlink_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_tag_catalog_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_tag_catalog_default_limit,
      "kernel_get_tag_catalog_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_tag_note_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_tag_note_default_limit,
      "kernel_get_tag_note_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_tag_tree_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_tag_tree_default_limit,
      "kernel_get_tag_tree_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_graph_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_graph_default_limit,
      "kernel_get_graph_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_query_tags_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or tag arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_tag_list tags{};
  const kernel_status status =
      kernel_query_tags(session->handle, static_cast<size_t>(limit), &tags);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_tags", out_error);
  }

  std::string json = "{\"tags\":[";
  for (size_t index = 0; index < tags.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    json += "{\"name\":\"" + JsonEscape(tags.tags[index].name) + "\",";
    json += "\"count\":" + std::to_string(tags.tags[index].count) + "}";
  }
  json += "],\"count\":" + std::to_string(tags.count) + "}";

  kernel_free_tag_list(&tags);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate tag summary JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_tag_notes_json(
    sealed_kernel_bridge_session* session,
    const char* tag_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      tag_utf8 == nullptr || tag_utf8[0] == '\0' || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or tag-note arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_search_results results{};
  const kernel_status status =
      kernel_query_tag_notes(session->handle, tag_utf8, static_cast<size_t>(limit), &results);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_tag_notes", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < results.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendNoteHitJson(json, results.hits[index].rel_path, results.hits[index].title);
  }
  json += "],\"count\":" + std::to_string(results.count) + "}";

  kernel_free_search_results(&results);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate tag-note JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_graph_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or graph arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_graph graph{};
  const kernel_status status =
      kernel_query_graph(session->handle, static_cast<size_t>(limit), &graph);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_graph", out_error);
  }

  std::string json = "{\"nodes\":[";
  for (size_t index = 0; index < graph.node_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const std::string node_id_utf8 = ActiveCodePageToUtf8(graph.nodes[index].id);
    json += "{\"id\":\"" + JsonEscape(node_id_utf8.c_str()) + "\",";
    json += "\"name\":\"" + JsonEscape(graph.nodes[index].name) + "\",";
    json += "\"ghost\":";
    json += graph.nodes[index].ghost != 0 ? "true" : "false";
    json += "}";
  }
  json += "],\"links\":[";
  for (size_t index = 0; index < graph.link_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const std::string source_utf8 = ActiveCodePageToUtf8(graph.links[index].source);
    const std::string target_utf8 = ActiveCodePageToUtf8(graph.links[index].target);
    json += "{\"source\":\"" + JsonEscape(source_utf8.c_str()) + "\",";
    json += "\"target\":\"" + JsonEscape(target_utf8.c_str()) + "\",";
    json += "\"kind\":\"" + JsonEscape(graph.links[index].kind) + "\"}";
  }
  json += "]}";

  kernel_free_graph(&graph);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate graph JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_backlinks_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      rel_path_utf8 == nullptr || rel_path_utf8[0] == '\0' || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or backlink arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  if (rel_path.empty()) {
    SetError(out_error, "rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_search_results results{};
  const kernel_status status = kernel_query_backlinks(
      session->handle,
      rel_path.c_str(),
      static_cast<size_t>(limit),
      &results);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_backlinks", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < results.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendNoteHitJson(json, results.hits[index].rel_path, results.hits[index].title);
  }
  json += "],\"count\":" + std::to_string(results.count) + "}";

  kernel_free_search_results(&results);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate backlink JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_chem_spectra_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or chemistry spectra arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_chem_spectrum_list spectra{};
  const kernel_status status =
      kernel_query_chem_spectra(session->handle, static_cast<size_t>(limit), &spectra);
  if (status.code != KERNEL_OK) {
    kernel_free_chem_spectrum_list(&spectra);
    return ReturnKernelError(status, "kernel_query_chem_spectra", out_error);
  }

  std::string json = "{\"spectra\":[";
  for (size_t index = 0; index < spectra.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendChemSpectrumJson(json, spectra.spectra[index]);
  }
  json += "],\"count\":" + std::to_string(spectra.count) + "}";

  kernel_free_chem_spectrum_list(&spectra);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate chemistry spectra JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_chem_spectra_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_chem_spectra_default_limit,
      "kernel_get_chem_spectra_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_chem_spectrum_json(
    sealed_kernel_bridge_session* session,
    const char* attachment_rel_path_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string attachment_rel_path = Utf8ToActiveCodePage(attachment_rel_path_utf8);
  if (attachment_rel_path.empty()) {
    SetError(out_error, "attachment_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_chem_spectrum_record spectrum{};
  const kernel_status status =
      kernel_get_chem_spectrum(session->handle, attachment_rel_path.c_str(), &spectrum);
  if (status.code != KERNEL_OK) {
    kernel_free_chem_spectrum_record(&spectrum);
    return ReturnKernelError(status, "kernel_get_chem_spectrum", out_error);
  }

  std::string json = "{\"spectrum\":";
  AppendChemSpectrumJson(json, spectrum);
  json += "}";

  kernel_free_chem_spectrum_record(&spectrum);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate chemistry spectrum JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_note_chem_spectrum_refs_json(
    sealed_kernel_bridge_session* session,
    const char* note_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or chemistry spectrum ref arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string note_rel_path = Utf8ToActiveCodePage(note_rel_path_utf8);
  if (note_rel_path.empty()) {
    SetError(out_error, "note_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_chem_spectrum_source_refs refs{};
  const kernel_status status = kernel_query_note_chem_spectrum_refs(
      session->handle,
      note_rel_path.c_str(),
      static_cast<size_t>(limit),
      &refs);
  if (status.code != KERNEL_OK) {
    kernel_free_chem_spectrum_source_refs(&refs);
    return ReturnKernelError(status, "kernel_query_note_chem_spectrum_refs", out_error);
  }

  std::string json = "{\"refs\":[";
  for (size_t index = 0; index < refs.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendChemSpectrumSourceRefJson(json, refs.refs[index]);
  }
  json += "],\"count\":" + std::to_string(refs.count) + "}";

  kernel_free_chem_spectrum_source_refs(&refs);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate chemistry spectrum refs JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_note_chem_spectrum_refs_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_note_chem_spectrum_refs_default_limit,
      "kernel_get_note_chem_spectrum_refs_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_query_chem_spectrum_referrers_json(
    sealed_kernel_bridge_session* session,
    const char* attachment_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or chemistry spectrum referrer arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string attachment_rel_path = Utf8ToActiveCodePage(attachment_rel_path_utf8);
  if (attachment_rel_path.empty()) {
    SetError(out_error, "attachment_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_chem_spectrum_referrers referrers{};
  const kernel_status status = kernel_query_chem_spectrum_referrers(
      session->handle,
      attachment_rel_path.c_str(),
      static_cast<size_t>(limit),
      &referrers);
  if (status.code != KERNEL_OK) {
    kernel_free_chem_spectrum_referrers(&referrers);
    return ReturnKernelError(status, "kernel_query_chem_spectrum_referrers", out_error);
  }

  std::string json = "{\"referrers\":[";
  for (size_t index = 0; index < referrers.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendChemSpectrumReferrerJson(json, referrers.referrers[index]);
  }
  json += "],\"count\":" + std::to_string(referrers.count) + "}";

  kernel_free_chem_spectrum_referrers(&referrers);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate chemistry spectrum referrers JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_chem_spectrum_referrers_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_chem_spectrum_referrers_default_limit,
      "kernel_get_chem_spectrum_referrers_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_parse_spectroscopy_text_json(
    const char* raw_utf8,
    uint64_t raw_size,
    const char* extension_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || raw_utf8 == nullptr || extension_utf8 == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_spectroscopy_data data{};
  const kernel_status status = kernel_parse_spectroscopy_text(
      raw_utf8,
      static_cast<size_t>(raw_size),
      extension_utf8,
      &data);
  if (status.code != KERNEL_OK) {
    SetError(out_error, SpectroscopyErrorToken(data.error));
    kernel_free_spectroscopy_data(&data);
    return static_cast<int32_t>(status.code);
  }

  std::string json;
  AppendSpectroscopyJson(json, data);
  kernel_free_spectroscopy_data(&data);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_truth_diff_json(
    const char* prev_content,
    uint64_t prev_size,
    const char* curr_content,
    uint64_t curr_size,
    const char* file_extension_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr ||
      file_extension_utf8 == nullptr ||
      (prev_size > 0 && prev_content == nullptr) ||
      (curr_size > 0 && curr_content == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_truth_diff_result result{};
  const kernel_status status = kernel_compute_truth_diff(
      prev_content,
      static_cast<size_t>(prev_size),
      curr_content,
      static_cast<size_t>(curr_size),
      file_extension_utf8,
      &result);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT
            ? "invalid_argument"
            : "truth_diff_failed");
    kernel_free_truth_diff_result(&result);
    return static_cast<int32_t>(status.code);
  }

  std::string validation_error;
  if (!ValidateTruthDiffJsonInput(result, validation_error)) {
    SetError(out_error, validation_error);
    kernel_free_truth_diff_result(&result);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendTruthDiffJson(json, result);
  kernel_free_truth_diff_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_semantic_context_text(
    const char* content,
    uint64_t content_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (content_size > 0 && content == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_semantic_context(
      content,
      static_cast<size_t>(content_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT
            ? "invalid_argument"
            : "semantic_context_failed");
    kernel_free_buffer(&buffer);
    return static_cast<int32_t>(status.code);
  }

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

int32_t sealed_kernel_bridge_get_semantic_context_min_bytes(
    uint64_t* out_bytes,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_semantic_context_min_bytes,
      "kernel_get_semantic_context_min_bytes",
      out_bytes,
      out_error);
}

int32_t sealed_kernel_bridge_get_rag_context_per_note_char_limit(
    uint64_t* out_chars,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_rag_context_per_note_char_limit,
      "kernel_get_rag_context_per_note_char_limit",
      out_chars,
      out_error);
}

int32_t sealed_kernel_bridge_get_embedding_text_char_limit(
    uint64_t* out_chars,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_embedding_text_char_limit,
      "kernel_get_embedding_text_char_limit",
      out_chars,
      out_error);
}

int32_t sealed_kernel_bridge_compute_truth_state_json(
    const char* const* note_ids_utf8,
    const int64_t* active_secs,
    uint64_t activity_count,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (
      out_json == nullptr || (activity_count > 0 && note_ids_utf8 == nullptr) ||
      (activity_count > 0 && active_secs == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<std::string> note_ids;
  note_ids.reserve(static_cast<std::size_t>(activity_count));
  std::vector<kernel_study_note_activity> activities;
  activities.reserve(static_cast<std::size_t>(activity_count));
  for (uint64_t index = 0; index < activity_count; ++index) {
    if (note_ids_utf8[index] == nullptr) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    note_ids.push_back(Utf8ToActiveCodePage(note_ids_utf8[index]));
    activities.push_back(kernel_study_note_activity{note_ids.back().c_str(), active_secs[index]});
  }

  kernel_truth_state_snapshot state{};
  const kernel_status status = kernel_compute_truth_state_from_activity(
      activities.empty() ? nullptr : activities.data(),
      static_cast<size_t>(activities.size()),
      &state);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_compute_truth_state_from_activity", out_error);
  }

  std::string json;
  AppendTruthStateJson(json, state);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_study_streak_days(
    const int64_t* day_buckets,
    uint64_t day_count,
    int64_t today_bucket,
    int64_t* out_streak_days,
    char** out_error) {
  if (out_streak_days == nullptr || (day_count > 0 && day_buckets == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_compute_study_streak_days(
      day_buckets,
      static_cast<size_t>(day_count),
      today_bucket,
      out_streak_days);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_compute_study_streak_days", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_study_heatmap_grid_json(
    const char* const* dates_utf8,
    const int64_t* active_secs,
    uint64_t day_count,
    int64_t now_epoch_secs,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (
      out_json == nullptr || (day_count > 0 && dates_utf8 == nullptr) ||
      (day_count > 0 && active_secs == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<std::string> dates;
  dates.reserve(static_cast<std::size_t>(day_count));
  std::vector<kernel_heatmap_day_activity> days;
  days.reserve(static_cast<std::size_t>(day_count));
  for (uint64_t index = 0; index < day_count; ++index) {
    if (dates_utf8[index] == nullptr) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    dates.push_back(Utf8ToActiveCodePage(dates_utf8[index]));
    days.push_back(kernel_heatmap_day_activity{dates.back().c_str(), active_secs[index]});
  }

  kernel_heatmap_grid grid{};
  const kernel_status status = kernel_build_study_heatmap_grid(
      days.empty() ? nullptr : days.data(),
      static_cast<size_t>(days.size()),
      now_epoch_secs,
      &grid);
  if (status.code != KERNEL_OK) {
    kernel_free_study_heatmap_grid(&grid);
    return ReturnKernelError(status, "kernel_build_study_heatmap_grid", out_error);
  }

  std::string json;
  AppendHeatmapGridJson(json, grid);
  kernel_free_study_heatmap_grid(&grid);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_generate_mock_retrosynthesis_json(
    const char* target_smiles_utf8,
    uint8_t depth,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || target_smiles_utf8 == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_retro_tree tree{};
  const kernel_status status =
      kernel_generate_mock_retrosynthesis(target_smiles_utf8, depth, &tree);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT ? "invalid_argument" : "retro_failed");
    kernel_free_retro_tree(&tree);
    return static_cast<int32_t>(status.code);
  }

  std::string validation_error;
  if (!ValidateRetroTreeJsonInput(tree, validation_error)) {
    SetError(out_error, validation_error);
    kernel_free_retro_tree(&tree);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendRetroTreeJson(json, tree);
  kernel_free_retro_tree(&tree);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_simulate_polymerization_kinetics_json(
    double m0,
    double i0,
    double cta0,
    double kd,
    double kp,
    double kt,
    double ktr,
    double time_max,
    uint64_t steps,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_polymerization_kinetics_params params{
      m0,
      i0,
      cta0,
      kd,
      kp,
      kt,
      ktr,
      time_max,
      static_cast<size_t>(steps)};
  kernel_polymerization_kinetics_result result{};
  const kernel_status status = kernel_simulate_polymerization_kinetics(&params, &result);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT ? "invalid_argument" : "kinetics_failed");
    kernel_free_polymerization_kinetics_result(&result);
    return static_cast<int32_t>(status.code);
  }

  std::string validation_error;
  if (!ValidateKineticsJsonInput(result, validation_error)) {
    SetError(out_error, validation_error);
    kernel_free_polymerization_kinetics_result(&result);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendKineticsJson(json, result);
  kernel_free_polymerization_kinetics_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_recalculate_stoichiometry_json(
    const double* mw,
    const double* eq,
    const double* moles,
    const double* mass,
    const double* volume,
    const double* density,
    const uint8_t* has_density,
    const uint8_t* is_reference,
    uint64_t count,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (count > 0 &&
      (mw == nullptr ||
       eq == nullptr ||
       moles == nullptr ||
       mass == nullptr ||
       volume == nullptr ||
       density == nullptr ||
       has_density == nullptr ||
       is_reference == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel_stoichiometry_row_input> input;
  input.reserve(static_cast<size_t>(count));
  for (uint64_t index = 0; index < count; ++index) {
    const size_t row_index = static_cast<size_t>(index);
    input.push_back(kernel_stoichiometry_row_input{
        mw[row_index],
        eq[row_index],
        moles[row_index],
        mass[row_index],
        volume[row_index],
        density[row_index],
        static_cast<uint8_t>(has_density[row_index] != 0),
        static_cast<uint8_t>(is_reference[row_index] != 0)});
  }

  std::vector<kernel_stoichiometry_row_output> output(static_cast<size_t>(count));
  const kernel_status status = kernel_recalculate_stoichiometry(
      input.empty() ? nullptr : input.data(),
      input.size(),
      output.empty() ? nullptr : output.data());
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT
            ? "invalid_argument"
            : "stoichiometry_failed");
    return static_cast<int32_t>(status.code);
  }

  std::string json;
  AppendStoichiometryJson(json, output);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_pdf_ink_default_tolerance(
    float* out_tolerance,
    char** out_error) {
  if (out_tolerance == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_get_pdf_ink_default_tolerance(out_tolerance);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_pdf_annotation_storage_key(
    const char* pdf_path_utf8,
    char** out_key,
    char** out_error) {
  if (out_key != nullptr) {
    *out_key = nullptr;
  }
  if (out_key == nullptr || pdf_path_utf8 == nullptr || pdf_path_utf8[0] == '\0') {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_compute_pdf_annotation_storage_key(pdf_path_utf8, &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  const std::string key =
      buffer.data == nullptr ? std::string() : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  *out_key = CopyString(key);
  if (*out_key == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_pdf_lightweight_hash(
    const uint8_t* head,
    uint64_t head_size,
    const uint8_t* tail,
    uint64_t tail_size,
    uint64_t file_size,
    char** out_hash,
    char** out_error) {
  if (out_hash != nullptr) {
    *out_hash = nullptr;
  }
  if (
      out_hash == nullptr || (head_size > 0 && head == nullptr) ||
      (tail_size > 0 && tail == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_compute_pdf_lightweight_hash(
      head,
      static_cast<size_t>(head_size),
      tail,
      static_cast<size_t>(tail_size),
      file_size,
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  const std::string hash =
      buffer.data == nullptr ? std::string() : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  *out_hash = CopyString(hash);
  if (*out_hash == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_smooth_ink_strokes_json(
    const float* xs,
    const float* ys,
    const float* pressures,
    const uint64_t* point_counts,
    const float* stroke_widths,
    uint64_t stroke_count,
    float tolerance,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (stroke_count > 0 && (point_counts == nullptr || stroke_widths == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t total_point_count = 0;
  std::vector<size_t> counts;
  counts.reserve(static_cast<size_t>(stroke_count));
  for (uint64_t stroke_index = 0; stroke_index < stroke_count; ++stroke_index) {
    const size_t count = static_cast<size_t>(point_counts[stroke_index]);
    counts.push_back(count);
    total_point_count += count;
  }
  if (total_point_count > 0 && (xs == nullptr || ys == nullptr || pressures == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel_ink_point> points;
  points.reserve(total_point_count);
  for (size_t point_index = 0; point_index < total_point_count; ++point_index) {
    points.push_back(kernel_ink_point{xs[point_index], ys[point_index], pressures[point_index]});
  }

  std::vector<kernel_ink_stroke_input> strokes;
  strokes.reserve(static_cast<size_t>(stroke_count));
  size_t point_offset = 0;
  for (uint64_t stroke_index = 0; stroke_index < stroke_count; ++stroke_index) {
    const size_t count = counts[static_cast<size_t>(stroke_index)];
    kernel_ink_stroke_input stroke{};
    stroke.points = count == 0 ? nullptr : points.data() + point_offset;
    stroke.point_count = count;
    stroke.stroke_width = stroke_widths[stroke_index];
    strokes.push_back(stroke);
    point_offset += count;
  }

  kernel_ink_smoothing_result result{};
  const kernel_status status = kernel_smooth_ink_strokes(
      strokes.empty() ? nullptr : strokes.data(),
      strokes.size(),
      tolerance,
      &result);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT ? "invalid_argument" : "ink_smoothing_failed");
    kernel_free_ink_smoothing_result(&result);
    return static_cast<int32_t>(status.code);
  }

  std::string validation_error;
  if (!ValidateInkSmoothingJsonInput(result, validation_error)) {
    SetError(out_error, validation_error);
    kernel_free_ink_smoothing_result(&result);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendInkSmoothingJson(json, result);
  kernel_free_ink_smoothing_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_molecular_preview_json(
    const char* raw_utf8,
    uint64_t raw_size,
    const char* extension_utf8,
    uint64_t max_atoms,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || raw_utf8 == nullptr || extension_utf8 == nullptr || max_atoms == 0) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_molecular_preview preview{};
  const kernel_status status = kernel_build_molecular_preview(
      raw_utf8,
      static_cast<size_t>(raw_size),
      extension_utf8,
      static_cast<size_t>(max_atoms),
      &preview);
  if (status.code != KERNEL_OK) {
    SetError(out_error, MolecularPreviewErrorToken(preview.error));
    kernel_free_molecular_preview(&preview);
    return static_cast<int32_t>(status.code);
  }

  std::string json;
  AppendMolecularPreviewJson(json, preview);
  kernel_free_molecular_preview(&preview);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_normalize_molecular_preview_atom_limit(
    uint64_t requested_atoms,
    uint64_t* out_atoms,
    char** out_error) {
  if (out_atoms == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t normalized = 0;
  const kernel_status status = kernel_normalize_molecular_preview_atom_limit(
      static_cast<size_t>(requested_atoms),
      &normalized);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_atoms = static_cast<uint64_t>(normalized);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_symmetry_atom_limit(
    uint64_t* out_atoms,
    char** out_error) {
  if (out_atoms == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_symmetry_atom_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_atoms = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_crystal_supercell_atom_limit(
    uint64_t* out_atoms,
    char** out_error) {
  if (out_atoms == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_crystal_supercell_atom_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_atoms = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_calculate_symmetry_json(
    const char* raw_utf8,
    uint64_t raw_size,
    const char* format_utf8,
    uint64_t max_atoms,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || raw_utf8 == nullptr || format_utf8 == nullptr || max_atoms == 0) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_symmetry_calculation_result result{};
  const kernel_status status = kernel_calculate_symmetry(
      raw_utf8,
      static_cast<size_t>(raw_size),
      format_utf8,
      static_cast<size_t>(max_atoms),
      &result);
  if (status.code != KERNEL_OK) {
    SetError(out_error, SymmetryCalculationErrorToken(result));
    kernel_free_symmetry_calculation_result(&result);
    return static_cast<int32_t>(status.code);
  }

  std::string json;
  AppendSymmetryJson(json, result);
  kernel_free_symmetry_calculation_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_lattice_from_cif_json(
    const char* raw_utf8,
    uint64_t raw_size,
    uint32_t nx,
    uint32_t ny,
    uint32_t nz,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || raw_utf8 == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_lattice_result result{};
  const kernel_status status = kernel_build_lattice_from_cif(
      raw_utf8,
      static_cast<size_t>(raw_size),
      nx,
      ny,
      nz,
      &result);
  if (status.code != KERNEL_OK) {
    SetError(out_error, CrystalLatticeErrorToken(result));
    kernel_free_lattice_result(&result);
    return static_cast<int32_t>(status.code);
  }
  if (result.atom_count > 0 && result.atoms == nullptr) {
    SetError(out_error, "lattice_missing_atoms");
    kernel_free_lattice_result(&result);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendCrystalLatticeJson(json, result);
  kernel_free_lattice_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_calculate_miller_plane_from_cif_json(
    const char* raw_utf8,
    uint64_t raw_size,
    int32_t h,
    int32_t k,
    int32_t l,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || raw_utf8 == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_cif_miller_plane_result result{};
  const kernel_status status = kernel_calculate_miller_plane_from_cif(
      raw_utf8,
      static_cast<size_t>(raw_size),
      h,
      k,
      l,
      &result);
  if (status.code != KERNEL_OK) {
    SetError(out_error, CrystalCifMillerErrorToken(result));
    return static_cast<int32_t>(status.code);
  }

  std::string json;
  AppendCrystalMillerPlaneJson(json, result.plane);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_create_folder(
    sealed_kernel_bridge_session* session,
    const char* folder_rel_path_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string folder_rel_path = Utf8ToActiveCodePage(folder_rel_path_utf8);
  if (folder_rel_path.empty()) {
    SetError(out_error, "folder_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_create_folder(session->handle, folder_rel_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_create_folder", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_delete_entry(
    sealed_kernel_bridge_session* session,
    const char* target_rel_path_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string target_rel_path = Utf8ToActiveCodePage(target_rel_path_utf8);
  if (target_rel_path.empty()) {
    SetError(out_error, "target_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_delete_entry(session->handle, target_rel_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_delete_entry", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_rename_entry(
    sealed_kernel_bridge_session* session,
    const char* source_rel_path_utf8,
    const char* new_name_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string source_rel_path = Utf8ToActiveCodePage(source_rel_path_utf8);
  const std::string new_name = Utf8ToActiveCodePage(new_name_utf8);
  if (source_rel_path.empty() || new_name.empty()) {
    SetError(out_error, "source_rel_path and new_name must be non-empty UTF-8 strings.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_rename_entry(session->handle, source_rel_path.c_str(), new_name.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_rename_entry", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_move_entry(
    sealed_kernel_bridge_session* session,
    const char* source_rel_path_utf8,
    const char* dest_folder_rel_path_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string source_rel_path = Utf8ToActiveCodePage(source_rel_path_utf8);
  const std::string dest_folder_rel_path =
      dest_folder_rel_path_utf8 == nullptr ? std::string() : Utf8ToActiveCodePage(dest_folder_rel_path_utf8);
  if (source_rel_path.empty()) {
    SetError(out_error, "source_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_move_entry(
      session->handle,
      source_rel_path.c_str(),
      dest_folder_rel_path.empty() ? nullptr : dest_folder_rel_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_move_entry", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}
