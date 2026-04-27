/* Reason: This file freezes the minimal C ABI data types shared between the kernel and any host. */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KERNEL_REVISION_MAX 80
#define KERNEL_SYMMETRY_POINT_GROUP_MAX 32

typedef struct kernel_handle kernel_handle;

typedef enum kernel_error_code {
  KERNEL_OK = 0,
  KERNEL_ERROR_INVALID_ARGUMENT = 1,
  KERNEL_ERROR_NOT_FOUND = 2,
  KERNEL_ERROR_CONFLICT = 3,
  KERNEL_ERROR_IO = 4,
  KERNEL_ERROR_INTERNAL = 5,
  KERNEL_ERROR_TIMEOUT = 6
} kernel_error_code;

typedef enum kernel_session_state {
  KERNEL_SESSION_CLOSED = 0,
  KERNEL_SESSION_OPEN = 1,
  KERNEL_SESSION_FAULTED = 2
} kernel_session_state;

typedef enum kernel_index_state {
  KERNEL_INDEX_UNAVAILABLE = 0,
  KERNEL_INDEX_CATCHING_UP = 1,
  KERNEL_INDEX_READY = 2,
  KERNEL_INDEX_REBUILDING = 3
} kernel_index_state;

typedef enum kernel_write_disposition {
  KERNEL_WRITE_WRITTEN = 0,
  KERNEL_WRITE_NO_OP = 1
} kernel_write_disposition;

typedef struct kernel_status {
  kernel_error_code code;
} kernel_status;

typedef struct kernel_state_snapshot {
  kernel_session_state session_state;
  kernel_index_state index_state;
  uint64_t indexed_note_count;
  uint64_t pending_recovery_ops;
} kernel_state_snapshot;

typedef struct kernel_rebuild_status_snapshot {
  uint8_t in_flight;
  uint8_t has_last_result;
  uint64_t current_generation;
  uint64_t last_completed_generation;
  uint64_t current_started_at_ns;
  kernel_error_code last_result_code;
  uint64_t last_result_at_ns;
} kernel_rebuild_status_snapshot;

typedef struct kernel_owned_buffer {
  char* data;
  size_t size;
} kernel_owned_buffer;

typedef struct kernel_note_metadata {
  uint64_t file_size;
  uint64_t mtime_ns;
  char content_revision[KERNEL_REVISION_MAX];
} kernel_note_metadata;

typedef struct kernel_note_record {
  char* rel_path;
  char* title;
  uint64_t file_size;
  uint64_t mtime_ns;
  char content_revision[KERNEL_REVISION_MAX];
} kernel_note_record;

typedef struct kernel_note_list {
  kernel_note_record* notes;
  size_t count;
} kernel_note_list;

typedef struct kernel_path_list {
  char** paths;
  size_t count;
} kernel_path_list;

typedef struct kernel_file_tree_note {
  char* rel_path;
  char* name;
  char* extension;
  uint64_t mtime_ns;
} kernel_file_tree_note;

typedef struct kernel_file_tree_node kernel_file_tree_node;

struct kernel_file_tree_node {
  char* name;
  char* full_name;
  char* relative_path;
  uint8_t is_folder;
  uint8_t has_note;
  kernel_file_tree_note note;
  kernel_file_tree_node* children;
  size_t child_count;
  uint32_t file_count;
};

typedef struct kernel_file_tree {
  kernel_file_tree_node* nodes;
  size_t count;
} kernel_file_tree;

typedef struct kernel_tag_record {
  char* name;
  uint32_t count;
} kernel_tag_record;

typedef struct kernel_tag_list {
  kernel_tag_record* tags;
  size_t count;
} kernel_tag_list;

typedef struct kernel_graph_node {
  char* id;
  char* name;
  uint8_t ghost;
} kernel_graph_node;

typedef struct kernel_graph_link {
  char* source;
  char* target;
  char* kind;
} kernel_graph_link;

typedef struct kernel_graph {
  kernel_graph_node* nodes;
  size_t node_count;
  kernel_graph_link* links;
  size_t link_count;
} kernel_graph;

typedef enum kernel_search_match_flags {
  KERNEL_SEARCH_MATCH_NONE = 0,
  KERNEL_SEARCH_MATCH_TITLE = 1,
  KERNEL_SEARCH_MATCH_BODY = 2,
  KERNEL_SEARCH_MATCH_PATH = 4
} kernel_search_match_flags;

typedef enum kernel_search_kind {
  KERNEL_SEARCH_KIND_NOTE = 0,
  KERNEL_SEARCH_KIND_ATTACHMENT = 1,
  KERNEL_SEARCH_KIND_ALL = 2
} kernel_search_kind;

typedef enum kernel_search_sort_mode {
  KERNEL_SEARCH_SORT_REL_PATH_ASC = 0,
  KERNEL_SEARCH_SORT_RANK_V1 = 1
} kernel_search_sort_mode;

typedef enum kernel_search_result_kind {
  KERNEL_SEARCH_RESULT_NOTE = 0,
  KERNEL_SEARCH_RESULT_ATTACHMENT = 1
} kernel_search_result_kind;

typedef enum kernel_search_snippet_status {
  KERNEL_SEARCH_SNIPPET_NONE = 0,
  KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED = 1,
  KERNEL_SEARCH_SNIPPET_TITLE_ONLY = 2,
  KERNEL_SEARCH_SNIPPET_UNAVAILABLE = 3
} kernel_search_snippet_status;

typedef enum kernel_search_result_flags {
  KERNEL_SEARCH_RESULT_FLAG_NONE = 0,
  KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING = 1
} kernel_search_result_flags;

typedef struct kernel_search_hit {
  char* rel_path;
  char* title;
  uint32_t match_flags;
} kernel_search_hit;

typedef struct kernel_search_results {
  kernel_search_hit* hits;
  size_t count;
} kernel_search_results;

typedef struct kernel_search_query {
  const char* query;
  size_t limit;
  size_t offset;
  kernel_search_kind kind;
  const char* tag_filter;
  const char* path_prefix;
  uint8_t include_deleted;
  kernel_search_sort_mode sort_mode;
} kernel_search_query;

typedef struct kernel_search_page_hit {
  char* rel_path;
  char* title;
  char* snippet;
  uint32_t match_flags;
  kernel_search_snippet_status snippet_status;
  kernel_search_result_kind result_kind;
  uint32_t result_flags;
  double score;
} kernel_search_page_hit;

typedef struct kernel_search_page {
  kernel_search_page_hit* hits;
  size_t count;
  uint64_t total_hits;
  uint8_t has_more;
} kernel_search_page;

typedef struct kernel_attachment_ref {
  char* rel_path;
} kernel_attachment_ref;

typedef struct kernel_attachment_refs {
  kernel_attachment_ref* refs;
  size_t count;
} kernel_attachment_refs;

typedef struct kernel_attachment_metadata {
  uint64_t file_size;
  uint64_t mtime_ns;
  uint8_t is_missing;
} kernel_attachment_metadata;

typedef enum kernel_attachment_presence {
  KERNEL_ATTACHMENT_PRESENCE_PRESENT = 0,
  KERNEL_ATTACHMENT_PRESENCE_MISSING = 1
} kernel_attachment_presence;

typedef enum kernel_attachment_kind {
  KERNEL_ATTACHMENT_KIND_UNKNOWN = 0,
  KERNEL_ATTACHMENT_KIND_GENERIC_FILE = 1,
  KERNEL_ATTACHMENT_KIND_IMAGE_LIKE = 2,
  KERNEL_ATTACHMENT_KIND_PDF_LIKE = 3,
  KERNEL_ATTACHMENT_KIND_CHEM_LIKE = 4
} kernel_attachment_kind;

typedef enum kernel_attachment_flags {
  KERNEL_ATTACHMENT_FLAG_NONE = 0
} kernel_attachment_flags;

typedef struct kernel_attachment_record {
  char* rel_path;
  char* basename;
  char* extension;
  uint64_t file_size;
  uint64_t mtime_ns;
  uint64_t ref_count;
  kernel_attachment_kind kind;
  uint32_t flags;
  kernel_attachment_presence presence;
} kernel_attachment_record;

typedef struct kernel_attachment_list {
  kernel_attachment_record* attachments;
  size_t count;
} kernel_attachment_list;

typedef struct kernel_attachment_referrer {
  char* note_rel_path;
  char* note_title;
} kernel_attachment_referrer;

typedef struct kernel_attachment_referrers {
  kernel_attachment_referrer* referrers;
  size_t count;
} kernel_attachment_referrers;

typedef enum kernel_pdf_metadata_state {
  KERNEL_PDF_METADATA_UNAVAILABLE = 0,
  KERNEL_PDF_METADATA_READY = 1,
  KERNEL_PDF_METADATA_PARTIAL = 2,
  KERNEL_PDF_METADATA_INVALID = 3
} kernel_pdf_metadata_state;

typedef enum kernel_pdf_doc_title_state {
  KERNEL_PDF_DOC_TITLE_UNAVAILABLE = 0,
  KERNEL_PDF_DOC_TITLE_ABSENT = 1,
  KERNEL_PDF_DOC_TITLE_AVAILABLE = 2
} kernel_pdf_doc_title_state;

typedef enum kernel_pdf_text_layer_state {
  KERNEL_PDF_TEXT_LAYER_UNAVAILABLE = 0,
  KERNEL_PDF_TEXT_LAYER_ABSENT = 1,
  KERNEL_PDF_TEXT_LAYER_PRESENT = 2
} kernel_pdf_text_layer_state;

typedef struct kernel_pdf_metadata_record {
  char* rel_path;
  char* doc_title;
  char* pdf_metadata_revision;
  uint64_t page_count;
  uint8_t has_outline;
  kernel_attachment_presence presence;
  kernel_pdf_metadata_state metadata_state;
  kernel_pdf_doc_title_state doc_title_state;
  kernel_pdf_text_layer_state text_layer_state;
} kernel_pdf_metadata_record;

typedef enum kernel_pdf_ref_state {
  KERNEL_PDF_REF_RESOLVED = 0,
  KERNEL_PDF_REF_MISSING = 1,
  KERNEL_PDF_REF_STALE = 2,
  KERNEL_PDF_REF_UNRESOLVED = 3
} kernel_pdf_ref_state;

typedef struct kernel_pdf_source_ref {
  char* pdf_rel_path;
  char* anchor_serialized;
  char* excerpt_text;
  uint64_t page;
  kernel_pdf_ref_state state;
} kernel_pdf_source_ref;

typedef struct kernel_pdf_source_refs {
  kernel_pdf_source_ref* refs;
  size_t count;
} kernel_pdf_source_refs;

typedef struct kernel_pdf_referrer {
  char* note_rel_path;
  char* note_title;
  char* anchor_serialized;
  char* excerpt_text;
  uint64_t page;
  kernel_pdf_ref_state state;
} kernel_pdf_referrer;

typedef struct kernel_pdf_referrers {
  kernel_pdf_referrer* referrers;
  size_t count;
} kernel_pdf_referrers;

typedef struct kernel_ink_point {
  float x;
  float y;
  float pressure;
} kernel_ink_point;

typedef struct kernel_ink_stroke_input {
  const kernel_ink_point* points;
  size_t point_count;
  float stroke_width;
} kernel_ink_stroke_input;

typedef struct kernel_ink_stroke {
  kernel_ink_point* points;
  size_t point_count;
  float stroke_width;
} kernel_ink_stroke;

typedef struct kernel_ink_smoothing_result {
  kernel_ink_stroke* strokes;
  size_t count;
} kernel_ink_smoothing_result;

typedef enum kernel_domain_carrier_kind {
  KERNEL_DOMAIN_CARRIER_ATTACHMENT = 0,
  KERNEL_DOMAIN_CARRIER_PDF = 1
} kernel_domain_carrier_kind;

typedef enum kernel_domain_value_kind {
  KERNEL_DOMAIN_VALUE_TOKEN = 0,
  KERNEL_DOMAIN_VALUE_BOOL = 1,
  KERNEL_DOMAIN_VALUE_UINT64 = 2,
  KERNEL_DOMAIN_VALUE_STRING = 3
} kernel_domain_value_kind;

typedef enum kernel_domain_metadata_flags {
  KERNEL_DOMAIN_METADATA_FLAG_NONE = 0
} kernel_domain_metadata_flags;

typedef struct kernel_domain_metadata_entry {
  kernel_domain_carrier_kind carrier_kind;
  char* carrier_key;
  char* namespace_name;
  uint32_t public_schema_revision;
  char* key_name;
  kernel_domain_value_kind value_kind;
  uint8_t bool_value;
  uint64_t uint64_value;
  char* string_value;
  uint32_t flags;
} kernel_domain_metadata_entry;

typedef struct kernel_domain_metadata_list {
  kernel_domain_metadata_entry* entries;
  size_t count;
} kernel_domain_metadata_list;

typedef enum kernel_domain_object_state {
  KERNEL_DOMAIN_OBJECT_PRESENT = 0,
  KERNEL_DOMAIN_OBJECT_MISSING = 1,
  KERNEL_DOMAIN_OBJECT_UNRESOLVED = 2,
  KERNEL_DOMAIN_OBJECT_UNSUPPORTED = 3
} kernel_domain_object_state;

typedef enum kernel_domain_object_flags {
  KERNEL_DOMAIN_OBJECT_FLAG_NONE = 0
} kernel_domain_object_flags;

typedef struct kernel_domain_object_descriptor {
  char* domain_object_key;
  kernel_domain_carrier_kind carrier_kind;
  char* carrier_key;
  char* subtype_namespace;
  char* subtype_name;
  uint32_t subtype_revision;
  kernel_attachment_kind coarse_kind;
  kernel_attachment_presence presence;
  kernel_domain_object_state state;
  uint32_t flags;
} kernel_domain_object_descriptor;

typedef struct kernel_domain_object_list {
  kernel_domain_object_descriptor* objects;
  size_t count;
} kernel_domain_object_list;

typedef enum kernel_chem_spectrum_format {
  KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN = 0,
  KERNEL_CHEM_SPECTRUM_FORMAT_JCAMP_DX = 1,
  KERNEL_CHEM_SPECTRUM_FORMAT_SPECTRUM_CSV_V1 = 2
} kernel_chem_spectrum_format;

typedef struct kernel_chem_spectrum_record {
  char* attachment_rel_path;
  char* domain_object_key;
  uint32_t subtype_revision;
  kernel_chem_spectrum_format source_format;
  kernel_attachment_kind coarse_kind;
  kernel_attachment_presence presence;
  kernel_domain_object_state state;
  uint32_t flags;
} kernel_chem_spectrum_record;

typedef struct kernel_chem_spectrum_list {
  kernel_chem_spectrum_record* spectra;
  size_t count;
} kernel_chem_spectrum_list;

typedef struct kernel_polymerization_kinetics_params {
  double m0;
  double i0;
  double cta0;
  double kd;
  double kp;
  double kt;
  double ktr;
  double time_max;
  size_t steps;
} kernel_polymerization_kinetics_params;

typedef struct kernel_polymerization_kinetics_result {
  double* time;
  double* conversion;
  double* mn;
  double* pdi;
  size_t count;
} kernel_polymerization_kinetics_result;

typedef struct kernel_stoichiometry_row_input {
  double mw;
  double eq;
  double moles;
  double mass;
  double volume;
  double density;
  uint8_t has_density;
  uint8_t is_reference;
} kernel_stoichiometry_row_input;

typedef struct kernel_stoichiometry_row_output {
  double mw;
  double eq;
  double moles;
  double mass;
  double volume;
  double density;
  uint8_t has_density;
  uint8_t is_reference;
} kernel_stoichiometry_row_output;

typedef enum kernel_truth_award_reason {
  KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA = 1,
  KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE = 2,
  KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT = 3
} kernel_truth_award_reason;

typedef struct kernel_truth_award {
  char* attr;
  int32_t amount;
  kernel_truth_award_reason reason;
  char* detail;
} kernel_truth_award;

typedef struct kernel_truth_diff_result {
  kernel_truth_award* awards;
  size_t count;
} kernel_truth_diff_result;

typedef struct kernel_study_note_activity {
  const char* note_id;
  int64_t active_secs;
} kernel_study_note_activity;

typedef struct kernel_study_stats_window {
  int64_t today_start_epoch_secs;
  int64_t today_bucket;
  int64_t week_start_epoch_secs;
  int64_t daily_window_start_epoch_secs;
  int64_t heatmap_start_epoch_secs;
  size_t folder_rank_limit;
} kernel_study_stats_window;

typedef struct kernel_truth_attribute_values {
  int64_t science;
  int64_t engineering;
  int64_t creation;
  int64_t finance;
} kernel_truth_attribute_values;

typedef struct kernel_truth_state_snapshot {
  int64_t level;
  int64_t total_exp;
  int64_t next_level_exp;
  kernel_truth_attribute_values attributes;
  kernel_truth_attribute_values attribute_exp;
} kernel_truth_state_snapshot;

typedef struct kernel_heatmap_day_activity {
  const char* date;
  int64_t active_secs;
} kernel_heatmap_day_activity;

typedef struct kernel_heatmap_cell {
  char* date;
  int64_t secs;
  size_t col;
  size_t row;
} kernel_heatmap_cell;

typedef struct kernel_heatmap_grid {
  kernel_heatmap_cell* cells;
  size_t count;
  int64_t max_secs;
  size_t weeks;
  size_t days_per_week;
} kernel_heatmap_grid;

typedef struct kernel_retro_precursor {
  char* id;
  char* smiles;
  char* role;
} kernel_retro_precursor;

typedef struct kernel_retro_pathway {
  char* target_id;
  char* reaction_name;
  char* conditions;
  kernel_retro_precursor* precursors;
  size_t precursor_count;
} kernel_retro_pathway;

typedef struct kernel_retro_tree {
  kernel_retro_pathway* pathways;
  size_t pathway_count;
} kernel_retro_tree;

typedef enum kernel_spectroscopy_parse_error {
  KERNEL_SPECTROSCOPY_PARSE_ERROR_NONE = 0,
  KERNEL_SPECTROSCOPY_PARSE_ERROR_UNSUPPORTED_EXTENSION = 1,
  KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_NUMERIC_ROWS = 2,
  KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_TOO_FEW_COLUMNS = 3,
  KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_VALID_POINTS = 4,
  KERNEL_SPECTROSCOPY_PARSE_ERROR_JDX_NO_POINTS = 5
} kernel_spectroscopy_parse_error;

typedef struct kernel_spectrum_series {
  double* y;
  size_t count;
  char* label;
} kernel_spectrum_series;

typedef struct kernel_spectroscopy_data {
  double* x;
  size_t x_count;
  kernel_spectrum_series* series;
  size_t series_count;
  char* x_label;
  char* title;
  uint8_t is_nmr;
  kernel_spectroscopy_parse_error error;
} kernel_spectroscopy_data;

typedef enum kernel_molecular_preview_error {
  KERNEL_MOLECULAR_PREVIEW_ERROR_NONE = 0,
  KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION = 1
} kernel_molecular_preview_error;

typedef struct kernel_molecular_preview {
  char* preview_data;
  size_t atom_count;
  size_t preview_atom_count;
  uint8_t truncated;
  kernel_molecular_preview_error error;
} kernel_molecular_preview;

typedef enum kernel_crystal_miller_error {
  KERNEL_CRYSTAL_MILLER_ERROR_NONE = 0,
  KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX = 1,
  KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL = 2,
  KERNEL_CRYSTAL_MILLER_ERROR_INVALID_BASIS = 3,
  KERNEL_CRYSTAL_MILLER_ERROR_ZERO_VOLUME = 4,
  KERNEL_CRYSTAL_MILLER_ERROR_ZERO_NORMAL = 5
} kernel_crystal_miller_error;

typedef struct kernel_crystal_cell_params {
  double a;
  double b;
  double c;
  double alpha_deg;
  double beta_deg;
  double gamma_deg;
} kernel_crystal_cell_params;

typedef enum kernel_crystal_parse_error {
  KERNEL_CRYSTAL_PARSE_ERROR_NONE = 0,
  KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL = 1,
  KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS = 2
} kernel_crystal_parse_error;

typedef struct kernel_fractional_atom_record {
  char* element;
  double frac[3];
} kernel_fractional_atom_record;

typedef struct kernel_miller_plane_result {
  double normal[3];
  double center[3];
  double d;
  double vertices[4][3];
  kernel_crystal_miller_error error;
} kernel_miller_plane_result;

typedef enum kernel_crystal_supercell_error {
  KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE = 0,
  KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL = 1,
  KERNEL_CRYSTAL_SUPERCELL_ERROR_INVALID_BASIS = 2,
  KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS = 3
} kernel_crystal_supercell_error;

typedef struct kernel_fractional_atom_input {
  const char* element;
  double frac[3];
} kernel_fractional_atom_input;

typedef struct kernel_symmetry_operation_input {
  double rot[3][3];
  double trans[3];
} kernel_symmetry_operation_input;

typedef struct kernel_crystal_parse_result {
  kernel_crystal_cell_params cell;
  kernel_fractional_atom_record* atoms;
  size_t atom_count;
  kernel_symmetry_operation_input* symops;
  size_t symop_count;
  kernel_crystal_parse_error error;
} kernel_crystal_parse_result;

typedef struct kernel_atom_node {
  char* element;
  double cartesian_coords[3];
} kernel_atom_node;

typedef struct kernel_supercell_result {
  kernel_atom_node* atoms;
  size_t count;
  uint64_t estimated_count;
  kernel_crystal_supercell_error error;
} kernel_supercell_result;

typedef struct kernel_unit_cell_box {
  double a;
  double b;
  double c;
  double alpha_deg;
  double beta_deg;
  double gamma_deg;
  double origin[3];
  double vectors[3][3];
} kernel_unit_cell_box;

typedef struct kernel_lattice_result {
  kernel_unit_cell_box unit_cell;
  kernel_atom_node* atoms;
  size_t atom_count;
  uint64_t estimated_count;
  kernel_crystal_parse_error parse_error;
  kernel_crystal_supercell_error supercell_error;
} kernel_lattice_result;

typedef struct kernel_cif_miller_plane_result {
  kernel_miller_plane_result plane;
  kernel_crystal_parse_error parse_error;
} kernel_cif_miller_plane_result;

typedef struct kernel_symmetry_axis_input {
  double dir[3];
  uint8_t order;
} kernel_symmetry_axis_input;

typedef struct kernel_symmetry_plane_input {
  double normal[3];
} kernel_symmetry_plane_input;

typedef struct kernel_symmetry_direction_input {
  double dir[3];
} kernel_symmetry_direction_input;

typedef struct kernel_symmetry_atom_input {
  const char* element;
  double position[3];
  double mass;
} kernel_symmetry_atom_input;

typedef struct kernel_symmetry_shape_result {
  double center_of_mass[3];
  double mol_radius;
  uint8_t is_linear;
  double linear_axis[3];
  uint8_t has_inversion;
} kernel_symmetry_shape_result;

typedef struct kernel_symmetry_render_axis {
  double vector[3];
  double center[3];
  uint8_t order;
  double start[3];
  double end[3];
} kernel_symmetry_render_axis;

typedef struct kernel_symmetry_render_plane {
  double normal[3];
  double center[3];
  double vertices[4][3];
} kernel_symmetry_render_plane;

typedef struct kernel_symmetry_classification_result {
  char point_group[KERNEL_SYMMETRY_POINT_GROUP_MAX];
} kernel_symmetry_classification_result;

typedef enum kernel_symmetry_parse_error {
  KERNEL_SYMMETRY_PARSE_ERROR_NONE = 0,
  KERNEL_SYMMETRY_PARSE_ERROR_UNSUPPORTED_FORMAT = 1,
  KERNEL_SYMMETRY_PARSE_ERROR_XYZ_EMPTY = 2,
  KERNEL_SYMMETRY_PARSE_ERROR_XYZ_INCOMPLETE = 3,
  KERNEL_SYMMETRY_PARSE_ERROR_XYZ_COORDINATE = 4,
  KERNEL_SYMMETRY_PARSE_ERROR_PDB_COORDINATE = 5,
  KERNEL_SYMMETRY_PARSE_ERROR_CIF_MISSING_CELL = 6,
  KERNEL_SYMMETRY_PARSE_ERROR_CIF_INVALID_CELL = 7
} kernel_symmetry_parse_error;

typedef struct kernel_symmetry_atom_record {
  char* element;
  double position[3];
  double mass;
} kernel_symmetry_atom_record;

typedef struct kernel_symmetry_atom_list {
  kernel_symmetry_atom_record* atoms;
  size_t count;
  kernel_symmetry_parse_error error;
} kernel_symmetry_atom_list;

typedef enum kernel_symmetry_calculation_error {
  KERNEL_SYMMETRY_CALC_ERROR_NONE = 0,
  KERNEL_SYMMETRY_CALC_ERROR_PARSE = 1,
  KERNEL_SYMMETRY_CALC_ERROR_NO_ATOMS = 2,
  KERNEL_SYMMETRY_CALC_ERROR_TOO_MANY_ATOMS = 3,
  KERNEL_SYMMETRY_CALC_ERROR_INTERNAL = 4
} kernel_symmetry_calculation_error;

typedef struct kernel_symmetry_calculation_result {
  char point_group[KERNEL_SYMMETRY_POINT_GROUP_MAX];
  kernel_symmetry_render_axis* axes;
  size_t axis_count;
  kernel_symmetry_render_plane* planes;
  size_t plane_count;
  uint8_t has_inversion;
  size_t atom_count;
  kernel_symmetry_calculation_error error;
  kernel_symmetry_parse_error parse_error;
} kernel_symmetry_calculation_result;

typedef enum kernel_chem_spectrum_selector_kind {
  KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM = 0,
  KERNEL_CHEM_SPECTRUM_SELECTOR_X_RANGE = 1
} kernel_chem_spectrum_selector_kind;

typedef enum kernel_domain_selector_kind {
  KERNEL_DOMAIN_SELECTOR_PAGE = 0,
  KERNEL_DOMAIN_SELECTOR_TEXT_EXCERPT = 1,
  KERNEL_DOMAIN_SELECTOR_TOKEN_REF = 2,
  KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR = 3
} kernel_domain_selector_kind;

typedef enum kernel_domain_ref_state {
  KERNEL_DOMAIN_REF_RESOLVED = 0,
  KERNEL_DOMAIN_REF_MISSING = 1,
  KERNEL_DOMAIN_REF_STALE = 2,
  KERNEL_DOMAIN_REF_UNRESOLVED = 3,
  KERNEL_DOMAIN_REF_UNSUPPORTED = 4
} kernel_domain_ref_state;

typedef enum kernel_domain_ref_flags {
  KERNEL_DOMAIN_REF_FLAG_NONE = 0
} kernel_domain_ref_flags;

typedef struct kernel_domain_source_ref {
  char* target_object_key;
  kernel_domain_selector_kind selector_kind;
  char* selector_serialized;
  char* preview_text;
  char* target_basis_revision;
  kernel_domain_ref_state state;
  uint32_t flags;
} kernel_domain_source_ref;

typedef struct kernel_domain_source_refs {
  kernel_domain_source_ref* refs;
  size_t count;
} kernel_domain_source_refs;

typedef struct kernel_domain_referrer {
  char* note_rel_path;
  char* note_title;
  char* target_object_key;
  kernel_domain_selector_kind selector_kind;
  char* selector_serialized;
  char* preview_text;
  char* target_basis_revision;
  kernel_domain_ref_state state;
  uint32_t flags;
} kernel_domain_referrer;

typedef struct kernel_domain_referrers {
  kernel_domain_referrer* referrers;
  size_t count;
} kernel_domain_referrers;

typedef struct kernel_chem_spectrum_source_ref {
  char* attachment_rel_path;
  char* domain_object_key;
  kernel_chem_spectrum_selector_kind selector_kind;
  char* selector_serialized;
  char* preview_text;
  char* target_basis_revision;
  kernel_domain_ref_state state;
  uint32_t flags;
} kernel_chem_spectrum_source_ref;

typedef struct kernel_chem_spectrum_source_refs {
  kernel_chem_spectrum_source_ref* refs;
  size_t count;
} kernel_chem_spectrum_source_refs;

typedef struct kernel_chem_spectrum_referrer {
  char* note_rel_path;
  char* note_title;
  char* attachment_rel_path;
  char* domain_object_key;
  kernel_chem_spectrum_selector_kind selector_kind;
  char* selector_serialized;
  char* preview_text;
  char* target_basis_revision;
  kernel_domain_ref_state state;
  uint32_t flags;
} kernel_chem_spectrum_referrer;

typedef struct kernel_chem_spectrum_referrers {
  kernel_chem_spectrum_referrer* referrers;
  size_t count;
} kernel_chem_spectrum_referrers;

#ifdef __cplusplus
}
#endif
