#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

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


int32_t sealed_kernel_bridge_normalize_pubchem_query_text(
    const char* query_utf8,
    uint64_t query_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (
      out_text == nullptr || (query_size > 0 && query_utf8 == nullptr) ||
      query_size > (std::numeric_limits<size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_normalize_pubchem_query(
      query_utf8,
      static_cast<size_t>(query_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_normalize_pubchem_query", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_build_pubchem_compound_info_json(
    const char* query_utf8,
    uint64_t query_size,
    const char* formula_utf8,
    uint64_t formula_size,
    double molecular_weight,
    uint8_t has_density,
    double density,
    uint64_t property_count,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  const auto exceeds_size_t = [](uint64_t value) {
    return value > (std::numeric_limits<size_t>::max)();
  };
  if (
      out_json == nullptr || (query_size > 0 && query_utf8 == nullptr) ||
      (formula_size > 0 && formula_utf8 == nullptr) || exceeds_size_t(query_size) ||
      exceeds_size_t(formula_size) || exceeds_size_t(property_count)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_pubchem_compound_info_json(
      query_utf8,
      static_cast<size_t>(query_size),
      formula_utf8,
      static_cast<size_t>(formula_size),
      molecular_weight,
      has_density,
      density,
      static_cast<size_t>(property_count),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_build_pubchem_compound_info_json", out_error);
  }

  return CopyKernelOwnedText(buffer, out_json, out_error);
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


