use std::os::raw::{c_char, c_double, c_float, c_int};

// Raw C ABI declarations for the sealed C++ kernel bridge.
// Keep this module boring: Rust-side behavior belongs in the parent module.

#[repr(C)]
pub(super) struct SealedKernelBridgeSession {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub(super) struct SealedKernelBridgeStateSnapshot {
    pub(super) session_state: i32,
    pub(super) index_state: i32,
    pub(super) indexed_note_count: u64,
    pub(super) pending_recovery_ops: u64,
}

extern "C" {
    pub(super) fn sealed_kernel_bridge_info_json() -> *mut c_char;
    pub(super) fn sealed_kernel_bridge_free_string(value: *mut c_char);
    pub(super) fn sealed_kernel_bridge_free_bytes(value: *mut u8);
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_free_float_array(value: *mut c_float);
    pub(super) fn sealed_kernel_bridge_open_vault_utf8(
        vault_path_utf8: *const c_char,
        out_session: *mut *mut SealedKernelBridgeSession,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_validate_vault_root_utf8(
        vault_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_close(session: *mut SealedKernelBridgeSession);
    pub(super) fn sealed_kernel_bridge_get_state(
        session: *mut SealedKernelBridgeSession,
        out_state: *mut SealedKernelBridgeStateSnapshot,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_note_catalog_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_note_query_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_vault_scan_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_notes_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_notes_filtered_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        ignored_roots_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_changed_notes_json(
        session: *mut SealedKernelBridgeSession,
        changed_paths_lf_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_file_tree_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_file_tree_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        ignored_roots_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_filter_supported_vault_paths_filtered_json(
        changed_paths_lf_utf8: *const c_char,
        ignored_roots_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_read_note_json(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_read_first_changed_markdown_note_content_text(
        session: *mut SealedKernelBridgeSession,
        changed_paths_lf_utf8: *const c_char,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_read_vault_file_bytes(
        session: *mut SealedKernelBridgeSession,
        host_path_utf8: *const c_char,
        host_path_size: u64,
        out_bytes: *mut *mut u8,
        out_size: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_write_note_json(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        content_utf8: *const c_char,
        content_size: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_search_notes_json(
        session: *mut SealedKernelBridgeSession,
        query_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_search_note_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_backlink_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_tag_catalog_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_tag_note_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_tag_tree_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_graph_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_tags_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_tag_tree_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_tag_notes_json(
        session: *mut SealedKernelBridgeSession,
        tag_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_graph_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_enriched_graph_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_backlinks_json(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_chem_spectra_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_chem_spectra_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_chem_spectrum_json(
        session: *mut SealedKernelBridgeSession,
        attachment_rel_path_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_note_chem_spectrum_refs_json(
        session: *mut SealedKernelBridgeSession,
        note_rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_note_chem_spectrum_refs_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_chem_spectrum_referrers_json(
        session: *mut SealedKernelBridgeSession,
        attachment_rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_chem_spectrum_referrers_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_parse_spectroscopy_text_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        extension_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_compute_truth_diff_json(
        prev_content: *const c_char,
        prev_size: u64,
        curr_content: *const c_char,
        curr_size: u64,
        file_extension_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_semantic_context_text(
        content: *const c_char,
        content_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_derive_file_extension_from_path_text(
        path_utf8: *const c_char,
        path_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_normalize_vault_relative_path_text(
        rel_path_utf8: *const c_char,
        rel_path_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_derive_note_display_name_from_path_text(
        path_utf8: *const c_char,
        path_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_normalize_database_column_type_text(
        column_type_utf8: *const c_char,
        column_type_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_normalize_database_json(
        json_utf8: *const c_char,
        json_size: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_paper_compile_plan_json(
        workspace_utf8: *const c_char,
        workspace_size: u64,
        template_utf8: *const c_char,
        template_size: u64,
        image_paths_utf8: *const *const c_char,
        image_path_sizes: *const u64,
        image_path_count: u64,
        csl_path_utf8: *const c_char,
        csl_path_size: u64,
        bibliography_path_utf8: *const c_char,
        bibliography_path_size: u64,
        resource_separator_utf8: *const c_char,
        resource_separator_size: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_default_paper_template(
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_summarize_paper_compile_log_json(
        log_utf8: *const c_char,
        log_size: u64,
        log_char_limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_normalize_pubchem_query_text(
        query_utf8: *const c_char,
        query_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_pubchem_compound_info_json(
        query_utf8: *const c_char,
        query_size: u64,
        formula_utf8: *const c_char,
        formula_size: u64,
        molecular_weight: c_double,
        has_density: u8,
        density: c_double,
        property_count: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_semantic_context_min_bytes(
        out_bytes: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_chat_timeout_secs(
        out_secs: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_ponder_timeout_secs(
        out_secs: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_embedding_request_timeout_secs(
        out_secs: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_embedding_cache_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_embedding_concurrency_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_rag_top_note_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_normalize_ai_embedding_text(
        text_utf8: *const c_char,
        text_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_compute_ai_embedding_cache_key(
        base_url_utf8: *const c_char,
        base_url_size: u64,
        model_utf8: *const c_char,
        model_size: u64,
        text_utf8: *const c_char,
        text_size: u64,
        out_key: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_serialize_ai_embedding_blob(
        values: *const c_float,
        value_count: u64,
        out_bytes: *mut *mut u8,
        out_size: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_parse_ai_embedding_blob(
        blob: *const u8,
        blob_size: u64,
        out_values: *mut *mut c_float,
        out_count: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_prepare_ai_embedding_refresh_jobs_json(
        session: *mut SealedKernelBridgeSession,
        ignored_roots_utf8: *const c_char,
        limit: u64,
        force_refresh: u8,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_prepare_changed_ai_embedding_refresh_jobs_json(
        session: *mut SealedKernelBridgeSession,
        changed_paths_lf_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_update_ai_embedding(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        values: *const c_float,
        value_count: u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_clear_ai_embeddings(
        session: *mut SealedKernelBridgeSession,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_delete_changed_ai_embedding_notes(
        session: *mut SealedKernelBridgeSession,
        changed_paths_lf_utf8: *const c_char,
        out_deleted_count: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_ai_embedding_top_notes_json(
        session: *mut SealedKernelBridgeSession,
        query_values: *const c_float,
        query_value_count: u64,
        exclude_rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_ai_rag_system_content_text(
        context_utf8: *const c_char,
        context_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_build_ai_rag_context_from_note_paths_text(
        note_paths_utf8: *const *const c_char,
        note_path_sizes: *const u64,
        note_contents_utf8: *const *const c_char,
        note_content_sizes: *const u64,
        note_count: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_ai_rag_context_from_changed_note_paths_text(
        session: *mut SealedKernelBridgeSession,
        note_paths_lf_utf8: *const c_char,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_ponder_system_prompt_text(
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_ai_ponder_user_prompt_text(
        topic_utf8: *const c_char,
        topic_size: u64,
        context_utf8: *const c_char,
        context_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_ai_ponder_temperature(
        out_temperature: *mut f32,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_compute_truth_state_json(
        note_ids_utf8: *const *const c_char,
        active_secs: *const i64,
        activity_count: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_compute_study_streak_days_from_timestamps(
        started_at_epoch_secs: *const i64,
        timestamp_count: u64,
        today_bucket: i64,
        out_streak_days: *mut i64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_compute_study_stats_window(
        now_epoch_secs: i64,
        days_back: i64,
        out_today_start_epoch_secs: *mut i64,
        out_today_bucket: *mut i64,
        out_week_start_epoch_secs: *mut i64,
        out_daily_window_start_epoch_secs: *mut i64,
        out_heatmap_start_epoch_secs: *mut i64,
        out_folder_rank_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    #[cfg(test)]
    pub(super) fn sealed_kernel_bridge_build_study_heatmap_grid_json(
        dates_utf8: *const *const c_char,
        active_secs: *const i64,
        day_count: u64,
        now_epoch_secs: i64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_start_study_session(
        session: *mut SealedKernelBridgeSession,
        note_id_utf8: *const c_char,
        folder_utf8: *const c_char,
        out_session_id: *mut i64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_tick_study_session(
        session: *mut SealedKernelBridgeSession,
        session_id: i64,
        active_secs: i64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_end_study_session(
        session: *mut SealedKernelBridgeSession,
        session_id: i64,
        active_secs: i64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_study_stats_json(
        session: *mut SealedKernelBridgeSession,
        now_epoch_secs: i64,
        days_back: i64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_study_truth_state_json(
        session: *mut SealedKernelBridgeSession,
        now_epoch_millis: i64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_query_study_heatmap_grid_json(
        session: *mut SealedKernelBridgeSession,
        now_epoch_secs: i64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_generate_mock_retrosynthesis_json(
        target_smiles_utf8: *const c_char,
        depth: u8,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_simulate_polymerization_kinetics_json(
        m0: f64,
        i0: f64,
        cta0: f64,
        kd: f64,
        kp: f64,
        kt: f64,
        ktr: f64,
        time_max: f64,
        steps: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_recalculate_stoichiometry_json(
        mw: *const f64,
        eq: *const f64,
        moles: *const f64,
        mass: *const f64,
        volume: *const f64,
        density: *const f64,
        has_density: *const u8,
        is_reference: *const u8,
        count: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_pdf_ink_default_tolerance(
        out_tolerance: *mut f32,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_compute_pdf_file_lightweight_hash(
        session: *mut SealedKernelBridgeSession,
        host_path_utf8: *const c_char,
        host_path_size: u64,
        out_hash: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_read_pdf_annotation_json(
        session: *mut SealedKernelBridgeSession,
        pdf_rel_path_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_write_pdf_annotation_json(
        session: *mut SealedKernelBridgeSession,
        pdf_rel_path_utf8: *const c_char,
        json_utf8: *const c_char,
        json_size: u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_smooth_ink_strokes_json(
        xs: *const f32,
        ys: *const f32,
        pressures: *const f32,
        point_counts: *const u64,
        stroke_widths: *const f32,
        stroke_count: u64,
        tolerance: f32,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_molecular_preview_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        extension_utf8: *const c_char,
        max_atoms: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_normalize_molecular_preview_atom_limit(
        requested_atoms: u64,
        out_atoms: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_symmetry_atom_limit(
        out_atoms: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_get_crystal_supercell_atom_limit(
        out_atoms: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_calculate_symmetry_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        format_utf8: *const c_char,
        max_atoms: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_build_lattice_from_cif_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        nx: u32,
        ny: u32,
        nz: u32,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_calculate_miller_plane_from_cif_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        h: i32,
        k: i32,
        l: i32,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_relativize_vault_path_text(
        session: *mut SealedKernelBridgeSession,
        host_path_utf8: *const c_char,
        host_path_size: u64,
        allow_empty: u8,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_create_folder(
        session: *mut SealedKernelBridgeSession,
        folder_rel_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_delete_entry(
        session: *mut SealedKernelBridgeSession,
        target_rel_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_rename_entry(
        session: *mut SealedKernelBridgeSession,
        source_rel_path_utf8: *const c_char,
        new_name_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    pub(super) fn sealed_kernel_bridge_move_entry(
        session: *mut SealedKernelBridgeSession,
        source_rel_path_utf8: *const c_char,
        dest_folder_rel_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
}
