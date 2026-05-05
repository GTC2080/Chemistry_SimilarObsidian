// Tests for the sealed kernel bridge live outside the runtime bridge file
// so the shell stays focused on production glue code.

use super::*;
use std::path::PathBuf;

#[test]
fn validate_rel_path_normalizes_windows_separators() {
    let value = validate_rel_path(" folder\\note.md ", "笔记").expect("valid rel path");
    assert_eq!(value, "folder/note.md");
}

#[test]
fn validate_rel_path_rejects_parent_segments() {
    assert!(validate_rel_path("folder/../note.md", "笔记").is_err());
    assert!(validate_rel_path("folder/./note.md", "笔记").is_err());
    assert!(validate_rel_path("folder//note.md", "笔记").is_err());
}

#[test]
fn validate_rel_path_rejects_rooted_paths() {
    assert!(validate_rel_path("/note.md", "笔记").is_err());
    assert!(validate_rel_path("C:/vault/note.md", "笔记").is_err());
}

#[test]
fn validate_rel_path_rejects_nul_bytes() {
    assert!(validate_rel_path("folder\0note.md", "笔记").is_err());
}

fn make_test_vault(prefix: &str) -> PathBuf {
    let nanos = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    let vault = std::env::temp_dir().join(format!("nexus-{prefix}-{}-{nanos}", std::process::id()));
    std::fs::create_dir_all(&vault).expect("create test vault");
    vault
}

fn close_test_vault(state: &SealedKernelState) {
    let existing = {
        let mut guard = state.session.lock().expect("lock session");
        guard.take()
    };
    if let Some(existing) = existing {
        unsafe { sealed_kernel_bridge_close(existing as *mut SealedKernelBridgeSession) };
    }
    let mut guard = state.vault_path.lock().expect("lock vault path");
    *guard = None;
}

#[test]
fn file_path_relativization_comes_from_kernel_handle() {
    let vault = make_test_vault("relativize");
    let vault_path = vault.to_string_lossy().into_owned();
    let state = SealedKernelState::default();
    open_vault_inner(vault_path.clone(), &state).expect("open test vault");

    let note_path = vault.join("Folder").join("Note.md");
    let note_path = note_path.to_string_lossy().into_owned();
    let rel_path = rel_path_from_file_path(&note_path, &state, "文件").expect("kernel rel path");
    assert_eq!(rel_path, "Folder/Note.md");

    let root_rel_path =
        rel_path_from_optional_folder_path(&vault_path, &state).expect("root folder path");
    assert_eq!(root_rel_path, "");

    let outside_path = vault
        .parent()
        .expect("vault parent")
        .join("outside.md")
        .to_string_lossy()
        .into_owned();
    assert!(rel_path_from_file_path(&outside_path, &state, "文件").is_err());

    close_test_vault(&state);
    let _ = std::fs::remove_dir_all(&vault);
}

#[test]
fn filter_supported_vault_paths_filtered_uses_kernel_hidden_and_ignored_rules() {
    let paths = vec![
        " Folder\\Note.md ".to_string(),
        "Folder/Note.md".to_string(),
        ".hidden/Note.md".to_string(),
        "Folder/.hidden/Note.md".to_string(),
        "node_modules/Note.md".to_string(),
        "Other.PDF".to_string(),
        "Other.exe".to_string(),
    ];

    assert_eq!(
        filter_supported_vault_paths_filtered(&paths, " node_modules ")
            .expect("kernel filtered supported path filter"),
        vec!["Folder/Note.md".to_string(), "Other.PDF".to_string()]
    );
}

#[test]
fn compute_truth_diff_uses_sealed_bridge_code_language_award() {
    let result = compute_truth_diff("note", "note\n```rust\nfn main() {}\n```", "md")
        .expect("sealed bridge truth diff");

    assert_eq!(result.len(), 1);
    assert_eq!(result[0].attr, "engineering");
    assert_eq!(result[0].amount, 8);
    assert_eq!(result[0].reason_key, "codeLanguage");
    assert_eq!(result[0].detail, "rust");
}

#[test]
fn build_semantic_context_uses_sealed_bridge_short_trim() {
    let result =
        build_semantic_context("  short note  \n").expect("sealed bridge semantic context");

    assert_eq!(result, "short note");
}

#[test]
fn parse_spectroscopy_from_text_uses_sealed_bridge_csv_parser() {
    let parsed = parse_spectroscopy_from_text(
        "ppm,intensity,fit\n1.0,5.0,4.5\n2.0,6.0,bad\n3.0,7.0\n",
        "csv",
    )
    .expect("sealed bridge csv parse");

    assert_eq!(parsed.x, vec![1.0, 2.0, 3.0]);
    assert_eq!(parsed.x_label, "ppm");
    assert!(parsed.is_nmr);
    assert_eq!(parsed.series.len(), 2);
    assert_eq!(parsed.series[0].label, "intensity");
    assert_eq!(parsed.series[0].y, vec![5.0, 6.0, 7.0]);
    assert_eq!(parsed.series[1].label, "fit");
    assert_eq!(parsed.series[1].y, vec![4.5, 0.0, 0.0]);
}

#[test]
fn parse_spectroscopy_from_text_uses_sealed_bridge_jdx_parser() {
    let parsed = parse_spectroscopy_from_text(
        "##TITLE=Sample NMR\n\
         ##DATATYPE=NMR SPECTRUM\n\
         ##XUNITS=PPM\n\
         ##YUNITS=INTENSITY\n\
         ##PEAK TABLE=(XY..XY)\n\
         1.0, 10.0; 2.0, 11.0\n\
         ##END=\n",
        "jdx",
    )
    .expect("sealed bridge jdx parse");

    assert_eq!(parsed.title, "Sample NMR");
    assert_eq!(parsed.x_label, "PPM");
    assert!(parsed.is_nmr);
    assert_eq!(parsed.x, vec![1.0, 2.0]);
    assert_eq!(parsed.series.len(), 1);
    assert_eq!(parsed.series[0].label, "INTENSITY");
    assert_eq!(parsed.series[0].y, vec![10.0, 11.0]);
}

#[test]
fn normalize_molecular_preview_atom_limit_uses_kernel_bounds() {
    assert_eq!(normalize_molecular_preview_atom_limit(0).unwrap(), 2000);
    assert_eq!(normalize_molecular_preview_atom_limit(2).unwrap(), 200);
    assert_eq!(normalize_molecular_preview_atom_limit(500).unwrap(), 500);
    assert_eq!(
        normalize_molecular_preview_atom_limit(50000).unwrap(),
        20000
    );
}

#[test]
fn compute_atom_limits_come_from_kernel() {
    assert_eq!(symmetry_atom_limit().unwrap(), 500);
    assert_eq!(crystal_supercell_atom_limit().unwrap(), 50000);
}

#[test]
fn note_catalog_default_limit_comes_from_kernel() {
    assert_eq!(note_catalog_default_limit().unwrap(), 100000);
}

#[test]
fn note_query_default_limit_comes_from_kernel() {
    assert_eq!(note_query_default_limit().unwrap(), 512);
}

#[test]
fn note_info_from_record_uses_kernel_extension_rules() {
    let note = note_info_from_record(
        "C:\\vault",
        SealedKernelNoteRecord {
            rel_path: "Folder\\Sample.İON".to_string(),
            title: "Fallback".to_string(),
            mtime_ns: 1_700_000_000_000_000_000,
        },
    );

    assert_eq!(note.id, "Folder/Sample.İON");
    assert_eq!(note.name, "Sample");
    assert_eq!(note.file_extension, "İon");
    assert_eq!(note.updated_at, 1_700_000_000);
}

#[test]
fn note_display_name_from_path_comes_from_kernel() {
    assert_eq!(
        derive_note_display_name_from_path("Folder/Alpha.md").unwrap(),
        "Alpha"
    );
    assert_eq!(
        derive_note_display_name_from_path("Lab\\Beta.MD").unwrap(),
        "Beta"
    );
    assert_eq!(
        derive_note_display_name_from_path("README").unwrap(),
        "README"
    );
    assert_eq!(derive_note_display_name_from_path(".env").unwrap(), ".env");
}

#[test]
fn product_text_limits_come_from_kernel() {
    assert_eq!(semantic_context_min_bytes().unwrap(), 24);
}

#[test]
fn file_extension_from_path_comes_from_kernel() {
    assert_eq!(
        derive_file_extension_from_path("Folder.v1/Spectrum.CSV").unwrap(),
        "csv"
    );
    assert_eq!(
        derive_file_extension_from_path("C:\\vault\\Mol.XYZ").unwrap(),
        "xyz"
    );
    assert_eq!(
        derive_file_extension_from_path("Folder.With.Dot/README").unwrap(),
        ""
    );
}

#[test]
fn database_column_type_normalization_comes_from_kernel() {
    assert_eq!(normalize_database_column_type("number").unwrap(), "number");
    assert_eq!(normalize_database_column_type("formula").unwrap(), "text");
    assert_eq!(normalize_database_column_type("").unwrap(), "text");
}

#[test]
fn paper_compile_plan_comes_from_kernel() {
    let image_paths = vec![
        "C:\\vault\\figs\\a.png".to_string(),
        "C:\\vault\\figs\\b.png".to_string(),
        "C:\\vault\\other\\c.png".to_string(),
        "loose.png".to_string(),
    ];
    let plan = build_paper_compile_plan(
        "E:\\tmp\\nexus-paper-1",
        " Standard-Thesis ",
        &image_paths,
        Some("  "),
        Some(" refs.bib "),
        ";",
    )
    .unwrap();

    assert_eq!(
        plan.template_args,
        vec![
            "-V".to_string(),
            "documentclass=report".to_string(),
            "-V".to_string(),
            "fontsize=12pt".to_string(),
            "-V".to_string(),
            "geometry:margin=1in".to_string()
        ]
    );
    assert_eq!(plan.csl_path, "");
    assert_eq!(plan.bibliography_path, "refs.bib");
    assert_eq!(
        plan.resource_path,
        "E:\\tmp\\nexus-paper-1;C:\\vault\\figs;C:\\vault\\other"
    );
}

#[test]
fn paper_compile_defaults_and_log_summary_come_from_kernel() {
    assert_eq!(default_paper_template().unwrap(), "standard-thesis");

    let mut log = String::new();
    for index in 1..=13 {
        log.push_str(&format!("Error line {}\n", index));
    }
    let summary = summarize_paper_compile_log(&log, 9).unwrap();
    assert!(summary.summary.contains("Error line 1"));
    assert!(summary.summary.contains("Error line 12"));
    assert!(!summary.summary.contains("Error line 13"));
    assert_eq!(summary.log_prefix, "Error lin");
    assert!(summary.truncated);

    let unicode_summary = summarize_paper_compile_log("错误ABC\nfatal: issue\n", 3).unwrap();
    assert_eq!(unicode_summary.log_prefix, "错误A");
}

#[test]
fn pubchem_query_and_compound_info_come_from_kernel() {
    assert_eq!(normalize_pubchem_query("  Water  ").unwrap(), "Water");
    assert!(normalize_pubchem_query("   ").is_err());

    let info = build_pubchem_compound_info("Water", " H2O ", 18.015, Some(0.997), 1).unwrap();
    assert_eq!(info.status, "ok");
    assert_eq!(info.name, "Water");
    assert_eq!(info.formula, "H2O");
    assert_eq!(info.molecular_weight, 18.015);
    assert_eq!(info.density, Some(0.997));

    let missing_density =
        build_pubchem_compound_info("Water", "H2O", 18.015, Some(-1.0), 1).unwrap();
    assert_eq!(missing_density.status, "ok");
    assert_eq!(missing_density.density, None);

    let not_found = build_pubchem_compound_info("Water", "", 0.0, None, 0).unwrap();
    assert_eq!(not_found.status, "notFound");

    let ambiguous = build_pubchem_compound_info("Water", "H2O", 18.015, None, 2).unwrap();
    assert_eq!(ambiguous.status, "ambiguous");
}

#[test]
fn ai_host_runtime_defaults_come_from_kernel() {
    assert_eq!(ai_chat_timeout_secs().unwrap(), 120);
    assert_eq!(ai_ponder_timeout_secs().unwrap(), 60);
    assert_eq!(ai_embedding_request_timeout_secs().unwrap(), 30);
    assert_eq!(ai_embedding_cache_limit().unwrap(), 64);
    assert_eq!(ai_embedding_concurrency_limit().unwrap(), 4);
    assert_eq!(ai_rag_top_note_limit().unwrap(), 5);
}

#[test]
fn ai_prompt_shapes_come_from_kernel() {
    let rag_system = build_ai_rag_system_content("上下文").unwrap();
    assert!(rag_system.contains("你是一个私人知识库的极客助手"));
    assert!(rag_system.contains("以下是相关笔记上下文"));
    assert!(rag_system.ends_with("上下文"));

    let ponder_system = ai_ponder_system_prompt().unwrap();
    assert!(ponder_system.contains("严格 JSON 数组"));
    assert!(ponder_system.contains("禁止输出 Markdown"));

    assert_eq!(
        build_ai_ponder_user_prompt("核心", "上下文").unwrap(),
        "核心概念: 核心\n上下文: 上下文\n请生成 3 到 5 个具备逻辑递进或补充关系的子节点。"
    );
    assert!((ai_ponder_temperature().unwrap() - 0.7).abs() < f32::EPSILON);
}

#[test]
fn ai_embedding_text_normalization_comes_from_kernel() {
    assert_eq!(
        normalize_ai_embedding_text("  useful text  ").unwrap(),
        "  useful text  "
    );

    let long_content = format!("{}Z", "你".repeat(2000));
    let normalized = normalize_ai_embedding_text(&long_content).unwrap();
    assert!(!normalized.contains('Z'));
    assert!(normalized.contains(&"你".repeat(2000)));

    let err = normalize_ai_embedding_text(" \t\n").expect_err("empty embedding input");
    assert_eq!(err.to_string(), "文本内容为空，跳过向量化");
}

#[test]
fn ai_embedding_cache_key_comes_from_kernel() {
    let key = compute_ai_embedding_cache_key(
        "https://api.example.test",
        "embed-small",
        "normalized text",
    )
    .unwrap();
    assert_eq!(key, "098c1904d3c511cf");
    assert_ne!(
        key,
        compute_ai_embedding_cache_key("https://api.example.test", "embed-small", "other text")
            .unwrap()
    );
}

#[test]
fn ai_embedding_blob_codec_comes_from_kernel() {
    let blob = serialize_ai_embedding_blob(&[1.0, -2.5, 0.25]).unwrap();
    assert_eq!(
        blob,
        vec![0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x20, 0xc0, 0x00, 0x00, 0x80, 0x3e]
    );
    assert_eq!(
        parse_ai_embedding_blob(&blob).unwrap(),
        vec![1.0, -2.5, 0.25]
    );
    assert!(parse_ai_embedding_blob(&[0x00, 0x01, 0x02]).is_err());
}

#[test]
fn ai_rag_context_shape_comes_from_kernel() {
    let context = build_ai_rag_context_from_note_paths(&[
        ("Alpha.md".to_string(), "first".to_string()),
        ("Blank.md".to_string(), " \n\t ".to_string()),
        ("Beta.md".to_string(), "second".to_string()),
    ])
    .unwrap();

    assert_eq!(
        context,
        "--- 笔记 1 《Alpha》 ---\nfirst\n\n--- 笔记 2 《Beta》 ---\nsecond\n\n"
    );

    let long_content = format!("{}Z", "你".repeat(1500));
    let context =
        build_ai_rag_context_from_note_paths(&[("Long.md".to_string(), long_content)]).unwrap();
    assert!(!context.contains('Z'));
    assert!(context.contains(&"你".repeat(1500)));
}

#[test]
fn ai_rag_context_display_names_come_from_kernel_paths() {
    let context = build_ai_rag_context_from_note_paths(&[
        ("Folder/Alpha.md".to_string(), "first".to_string()),
        ("Lab\\Beta.MD".to_string(), "second".to_string()),
        ("README".to_string(), "third".to_string()),
    ])
    .unwrap();

    assert_eq!(
        context,
        "--- 笔记 1 《Alpha》 ---\nfirst\n\n--- 笔记 2 《Beta》 ---\nsecond\n\n--- 笔记 3 《README》 ---\nthird\n\n"
    );
}

#[test]
fn compute_truth_state_uses_kernel_activity_rules() {
    let state = compute_truth_state_from_activity(&[
        ("lab.csv".to_string(), 120),
        ("code.rs".to_string(), 3600),
        ("molecule.mol".to_string(), 3000),
        ("ledger.base".to_string(), 6000),
    ])
    .unwrap();

    assert_eq!(state.level, 2);
    assert_eq!(state.total_exp, 112);
    assert_eq!(state.next_level_exp, 150);
    assert_eq!(state.attribute_exp.science, 2);
    assert_eq!(state.attribute_exp.engineering, 60);
    assert_eq!(state.attribute_exp.creation, 50);
    assert_eq!(state.attribute_exp.finance, 100);
    assert_eq!(state.attributes.science, 1);
    assert_eq!(state.attributes.engineering, 2);
    assert_eq!(state.attributes.creation, 2);
    assert_eq!(state.attributes.finance, 3);
}

#[test]
fn compute_study_streak_from_timestamps_uses_kernel_bucket_rules() {
    assert_eq!(
        compute_study_streak_days_from_timestamps(&[1728005, 1641720, 1555200, 1728400], 20)
            .unwrap(),
        3
    );
    assert_eq!(
        compute_study_streak_days_from_timestamps(&[-1], -1).unwrap(),
        1
    );
}

#[test]
fn compute_study_stats_window_uses_kernel_calendar_rules() {
    let window = compute_study_stats_window(1714305600, 7).unwrap();

    assert_eq!(window.today_start_epoch_secs, 1714262400);
    assert_eq!(window.today_bucket, 19841);
    assert_eq!(window.week_start_epoch_secs, 1713744000);
    assert_eq!(window.daily_window_start_epoch_secs, 1713744000);
    assert_eq!(window.heatmap_start_epoch_secs, 1698796800);
    assert_eq!(window.folder_rank_limit, 5);
}

#[test]
fn build_study_heatmap_grid_uses_kernel_calendar_rules() {
    let grid = build_study_heatmap_grid(
        &[
            ("2023-10-30".to_string(), 60),
            ("2024-01-01".to_string(), 120),
            ("2024-01-01".to_string(), 30),
            ("2024-04-28".to_string(), 300),
            ("2022-01-01".to_string(), 999),
        ],
        1714305600,
    )
    .unwrap();

    assert_eq!(grid.cells.len(), 182);
    assert_eq!(grid.max_secs, 300);
    assert_eq!(grid.cells[0].date, "2023-10-30");
    assert_eq!(grid.cells[0].secs, 60);
    assert_eq!(grid.cells[0].col, 0);
    assert_eq!(grid.cells[0].row, 0);
    assert_eq!(grid.cells[63].date, "2024-01-01");
    assert_eq!(grid.cells[63].secs, 150);
    assert_eq!(grid.cells[181].date, "2024-04-28");
    assert_eq!(grid.cells[181].secs, 300);
    assert_eq!(grid.cells[181].col, 25);
    assert_eq!(grid.cells[181].row, 6);
}

#[test]
fn vault_scan_default_limit_comes_from_kernel() {
    assert_eq!(vault_scan_default_limit().unwrap(), 4096);
}

#[test]
fn file_tree_default_limit_comes_from_kernel() {
    assert_eq!(file_tree_default_limit().unwrap(), 4096);
}

#[test]
fn relationship_default_limits_come_from_kernel() {
    assert_eq!(search_note_default_limit().unwrap(), 10);
    assert_eq!(backlink_default_limit().unwrap(), 64);
    assert_eq!(tag_catalog_default_limit().unwrap(), 512);
    assert_eq!(tag_note_default_limit().unwrap(), 128);
    assert_eq!(tag_tree_default_limit().unwrap(), 512);
    assert_eq!(graph_default_limit().unwrap(), 2048);
}

#[test]
fn chemistry_spectrum_default_limits_come_from_kernel() {
    assert_eq!(chem_spectra_default_limit().unwrap(), 512);
    assert_eq!(note_chem_spectrum_refs_default_limit().unwrap(), 512);
    assert_eq!(chem_spectrum_referrers_default_limit().unwrap(), 512);
}

#[test]
fn parse_spectroscopy_from_text_maps_sealed_bridge_parse_errors() {
    let err = parse_spectroscopy_from_text("name,value\nnot-a-number,still-bad\n", "csv")
        .expect_err("invalid csv");
    assert_eq!(err.to_string(), "CSV 中未找到有效的数值数据行");

    let err = parse_spectroscopy_from_text("1,2\n", "txt").expect_err("unsupported");
    assert_eq!(err.to_string(), "不支持的波谱文件扩展名: txt");
}

#[test]
fn generate_mock_retrosynthesis_uses_sealed_bridge_amide_rules() {
    let result = generate_mock_retrosynthesis(" CC(=O)NCC1=CC=CC=C1 ", 2)
        .expect("sealed bridge retrosynthesis");

    assert!(!result.pathways.is_empty());
    assert_eq!(result.pathways[0].reaction_name, "Amide Coupling");
    assert!(result.pathways[0].target_id.starts_with("retro_"));
    assert_eq!(result.pathways[0].precursors[2].role, "reagent");
}

#[test]
fn generate_mock_retrosynthesis_maps_sealed_bridge_invalid_argument() {
    let err = generate_mock_retrosynthesis("   ", 2).expect_err("empty target");

    assert_eq!(err.to_string(), "请输入目标分子 SMILES");
}

fn default_kinetics_params() -> KineticsParams {
    KineticsParams {
        m0: 1.0,
        i0: 0.01,
        cta0: 0.001,
        kd: 0.001,
        kp: 100.0,
        kt: 1000.0,
        ktr: 0.1,
        time_max: 3600.0,
        steps: 120,
    }
}

#[test]
fn simulate_polymerization_kinetics_uses_sealed_bridge_series() {
    let params = default_kinetics_params();
    let result = simulate_polymerization_kinetics(params.clone()).expect("sealed bridge kinetics");

    assert_eq!(result.time.len(), params.steps + 1);
    assert_eq!(result.conversion.len(), result.time.len());
    assert_eq!(result.mn.len(), result.time.len());
    assert_eq!(result.pdi.len(), result.time.len());
    assert_eq!(result.time[0], 0.0);
    assert!((result.time[result.time.len() - 1] - params.time_max).abs() < 1.0e-9);
    assert!(result
        .pdi
        .iter()
        .all(|value| value.is_finite() && *value >= 1.0));
}

#[test]
fn simulate_polymerization_kinetics_maps_invalid_argument() {
    let mut params = default_kinetics_params();
    params.m0 = 0.0;

    let err = simulate_polymerization_kinetics(params).expect_err("invalid kinetics params");

    assert_eq!(err.to_string(), "聚合动力学参数无效");
}

fn stoichiometry_input(
    mw: f64,
    eq: f64,
    moles: f64,
    mass: f64,
    volume: f64,
    is_reference: bool,
    density: Option<f64>,
) -> SealedKernelStoichiometryInput {
    SealedKernelStoichiometryInput {
        mw,
        eq,
        moles,
        mass,
        volume,
        density: density.unwrap_or(0.0),
        has_density: density.is_some(),
        is_reference,
    }
}

#[test]
fn recalculate_stoichiometry_uses_sealed_bridge_reference_row() {
    let result = recalculate_stoichiometry(&[
        stoichiometry_input(50.0, 2.0, 99.0, 0.0, 0.0, false, None),
        stoichiometry_input(100.0, 7.0, 0.25, 9.0, 3.0, true, Some(2.0)),
        stoichiometry_input(10.0, 3.0, 99.0, 8.0, 4.0, true, None),
    ])
    .expect("sealed bridge stoichiometry");

    assert_eq!(result.len(), 3);
    assert!(!result[0].is_reference);
    assert!(result[1].is_reference);
    assert!(!result[2].is_reference);
    assert_eq!(result[1].eq, 1.0);
    assert_eq!(result[1].moles, 0.25);
    assert_eq!(result[1].mass, 25.0);
    assert!(result[1].has_density);
    assert_eq!(result[1].density, 2.0);
    assert_eq!(result[1].volume, 12.5);
    assert_eq!(result[2].eq, 3.0);
    assert_eq!(result[2].moles, 0.75);
    assert_eq!(result[2].mass, 7.5);
    assert!(result[2].has_density);
    assert_eq!(result[2].density, 2.0);
    assert_eq!(result[2].volume, 3.75);
}

#[test]
fn recalculate_stoichiometry_delegates_empty_rows_through_sealed_bridge() {
    let result = recalculate_stoichiometry(&[]).expect("sealed bridge empty stoichiometry");

    assert!(result.is_empty());
}

#[test]
fn smooth_ink_strokes_uses_sealed_bridge_series() {
    let strokes = vec![RawStroke {
        points: vec![
            crate::pdf::annotations::InkPoint {
                x: 0.0,
                y: 0.0,
                pressure: 0.5,
            },
            crate::pdf::annotations::InkPoint {
                x: 0.5,
                y: 0.2,
                pressure: 0.7,
            },
            crate::pdf::annotations::InkPoint {
                x: 1.0,
                y: 0.0,
                pressure: 0.9,
            },
        ],
        stroke_width: 0.01,
    }];

    let smoothed = smooth_ink_strokes(strokes, 0.001).expect("sealed bridge ink smoothing");

    assert_eq!(smoothed.len(), 1);
    assert!(smoothed[0].points.len() > 3);
    assert_eq!(smoothed[0].stroke_width, 0.01);
    assert!((smoothed[0].points[0].x - 0.0).abs() < 1.0e-6);
    assert!((smoothed[0].points.last().unwrap().x - 1.0).abs() < 1.0e-6);
}

#[test]
fn smooth_ink_strokes_preserves_two_point_strokes_through_sealed_bridge() {
    let strokes = vec![RawStroke {
        points: vec![
            crate::pdf::annotations::InkPoint {
                x: 0.0,
                y: 0.0,
                pressure: 0.5,
            },
            crate::pdf::annotations::InkPoint {
                x: 1.0,
                y: 1.0,
                pressure: 0.8,
            },
        ],
        stroke_width: 0.02,
    }];

    let smoothed = smooth_ink_strokes(strokes, 0.1).expect("sealed bridge ink smoothing");

    assert_eq!(smoothed[0].points.len(), 2);
    assert_eq!(smoothed[0].points[1].pressure, 0.8);
}

#[test]
fn pdf_ink_default_tolerance_comes_from_kernel() {
    assert!((pdf_ink_default_tolerance().unwrap() - 0.002).abs() < 1.0e-7);
}

#[test]
fn pdf_file_lightweight_hash_reads_vault_file_in_kernel() {
    let vault = make_test_vault("pdf-file-hash");
    let vault_path = vault.to_string_lossy().into_owned();
    let state = SealedKernelState::default();
    open_vault_inner(vault_path, &state).expect("open test vault");

    let pdf_path = vault.join("assets").join("paper.pdf");
    std::fs::create_dir_all(pdf_path.parent().expect("pdf parent")).expect("create pdf parent");
    std::fs::write(&pdf_path, b"%PDFEOF").expect("write pdf fixture");

    let session = active_session_token(&state).expect("active session");
    let hash = compute_pdf_file_lightweight_hash_for_session(session, &pdf_path.to_string_lossy())
        .expect("kernel pdf file hash");
    assert_eq!(
        hash,
        "a1ffd07a77f81b588d8a7184f7db683eb2cecd3148cf7e49cb1dcce24dcb41a8"
    );

    close_test_vault(&state);
    let _ = std::fs::remove_dir_all(&vault);
}

#[test]
fn pdf_annotation_json_io_uses_kernel_storage() {
    let vault = make_test_vault("pdf-annotation-json");
    let vault_path = vault.to_string_lossy().into_owned();
    let state = SealedKernelState::default();
    open_vault_inner(vault_path, &state).expect("open test vault");

    let session = active_session_token(&state).expect("active session");
    let json = r#"{"pdfPath":"docs/Paper.pdf","pdfHash":"hash","annotations":[]}"#;
    write_pdf_annotation_json_for_session(session, "docs/Paper.pdf", json)
        .expect("kernel write annotation json");
    assert_eq!(
        read_pdf_annotation_json_for_session(session, "docs/Paper.pdf")
            .expect("kernel read annotation json"),
        json
    );
    assert_eq!(
        read_pdf_annotation_json_for_session(session, "docs/Missing.pdf")
            .expect("kernel read missing annotation json"),
        ""
    );

    close_test_vault(&state);
    let _ = std::fs::remove_dir_all(&vault);
}

#[test]
fn calculate_symmetry_from_text_uses_sealed_bridge_water_pipeline() {
    let xyz = "3\nwater\nO  0.000  0.000  0.117\nH  0.000  0.757 -0.469\nH  0.000 -0.757 -0.469\n";
    let result = calculate_symmetry_from_text(xyz, "xyz").expect("sealed bridge symmetry");

    assert_eq!(result.point_group, "C_2v");
    assert!(!result.axes.is_empty());
    assert!(!result.planes.is_empty());
}

#[test]
fn calculate_symmetry_from_text_uses_sealed_bridge_linear_pipeline() {
    let xyz = "3\nCO2\nC  0.000  0.000  0.000\nO  0.000  0.000  1.160\nO  0.000  0.000 -1.160\n";
    let result = calculate_symmetry_from_text(xyz, "xyz").expect("sealed bridge symmetry");

    assert_eq!(result.point_group, "D∞h");
    assert!(result.has_inversion);
}

#[test]
fn calculate_symmetry_from_text_maps_sealed_bridge_parse_errors() {
    let err = calculate_symmetry_from_text("1\n", "xyz").expect_err("invalid xyz");
    assert_eq!(err.to_string(), "XYZ 文件格式不完整");

    let err = calculate_symmetry_from_text("1,2\n", "mol2").expect_err("unsupported");
    assert_eq!(err.to_string(), "不支持的分子文件格式: mol2");
}

#[test]
fn build_lattice_from_cif_uses_sealed_bridge_full_result() {
    let cif = r#"
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
"#;
    let data = build_lattice_from_cif(cif, 2, 2, 2).expect("sealed bridge lattice");
    assert_eq!(data.atoms.len(), 16);
    assert!((data.unit_cell.a - 5.64).abs() < 1e-8);

    let plane = calculate_miller_plane_from_cif(cif, 1, 1, 0).expect("sealed bridge Miller plane");
    assert!(plane.normal[2].abs() < 1e-6);
}

#[test]
fn build_lattice_from_cif_maps_sealed_bridge_parse_errors() {
    let cif = r#"
data_test
loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
"#;

    let err = build_lattice_from_cif(cif, 1, 1, 1).expect_err("missing cell");
    assert!(err.to_string().contains("晶胞参数"));

    let err = calculate_miller_plane_from_cif(cif, 1, 0, 0).expect_err("missing cell");
    assert!(err.to_string().contains("晶胞参数"));
}

#[test]
fn calculate_miller_plane_from_cif_maps_sealed_bridge_miller_errors() {
    let cif = r#"
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
"#;

    let err = calculate_miller_plane_from_cif(cif, 0, 0, 0).expect_err("zero index");
    assert_eq!(err.to_string(), "密勒指数 (h, k, l) 不能全为零");
}
