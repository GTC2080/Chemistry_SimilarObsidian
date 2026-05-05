use serde::{Deserialize, Serialize};

// JSON-facing DTOs used by the sealed kernel bridge.
// Runtime bridge behavior belongs in the parent module.
#[derive(Deserialize, Serialize)]
pub(super) struct SealedKernelNoteCatalog {
    pub(super) notes: Vec<SealedKernelNoteRecord>,
}

#[derive(Deserialize, Serialize)]
pub(super) struct SealedKernelNoteRecord {
    pub(super) rel_path: String,
    pub(super) title: String,
    #[serde(default)]
    pub(super) mtime_ns: u64,
}

#[derive(Debug, Clone)]
pub struct AiEmbeddingRefreshJob {
    pub id: String,
    pub content: String,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelAiEmbeddingRefreshJobCatalog {
    pub(super) jobs: Vec<SealedKernelAiEmbeddingRefreshJob>,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelAiEmbeddingRefreshJob {
    #[serde(rename = "relPath")]
    pub(super) rel_path: String,
    pub(super) content: String,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelTagCatalog {
    pub(super) tags: Vec<SealedKernelTagRecord>,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelTagRecord {
    pub(super) name: String,
    pub(super) count: u32,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelTagTreeCatalog {
    pub(super) nodes: Vec<SealedKernelTagTreeNode>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub(super) struct SealedKernelTagTreeNode {
    pub(super) name: String,
    pub(super) full_path: String,
    pub(super) count: u32,
    pub(super) children: Vec<SealedKernelTagTreeNode>,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelReadNoteResult {
    pub(super) content: String,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelFileTreeCatalog {
    pub(super) nodes: Vec<SealedKernelFileTreeNode>,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelPathCatalog {
    pub(super) paths: Vec<String>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelTruthAward {
    pub attr: String,
    pub amount: i32,
    pub reason_key: String,
    #[serde(default)]
    pub detail: String,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelTruthDiffResult {
    pub(super) awards: Vec<SealedKernelTruthAward>,
}

#[cfg(test)]
#[derive(Debug, Clone, Deserialize)]
pub struct SealedKernelTruthAttributes {
    pub science: i64,
    pub engineering: i64,
    pub creation: i64,
    pub finance: i64,
}

#[cfg(test)]
#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelTruthState {
    pub level: i64,
    pub total_exp: i64,
    pub next_level_exp: i64,
    pub attributes: SealedKernelTruthAttributes,
    pub attribute_exp: SealedKernelTruthAttributes,
}

#[cfg(test)]
#[derive(Debug, Clone, Deserialize)]
pub struct SealedKernelHeatmapCell {
    pub date: String,
    pub secs: i64,
    pub col: usize,
    pub row: usize,
}

#[cfg(test)]
#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelHeatmapGrid {
    pub cells: Vec<SealedKernelHeatmapCell>,
    pub max_secs: i64,
}

#[cfg(test)]
#[derive(Debug, Clone, Copy)]
pub struct SealedKernelStudyStatsWindow {
    pub today_start_epoch_secs: i64,
    pub today_bucket: i64,
    pub week_start_epoch_secs: i64,
    pub daily_window_start_epoch_secs: i64,
    pub heatmap_start_epoch_secs: i64,
    pub folder_rank_limit: u64,
}

#[derive(Debug, Clone)]
pub struct SealedKernelStoichiometryInput {
    pub mw: f64,
    pub eq: f64,
    pub moles: f64,
    pub mass: f64,
    pub volume: f64,
    pub density: f64,
    pub has_density: bool,
    pub is_reference: bool,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelStoichiometryRow {
    pub mw: f64,
    pub eq: f64,
    pub moles: f64,
    pub mass: f64,
    pub volume: f64,
    pub density: f64,
    pub has_density: bool,
    pub is_reference: bool,
}

#[derive(Deserialize)]
pub(super) struct SealedKernelStoichiometryResult {
    pub(super) rows: Vec<SealedKernelStoichiometryRow>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PaperCompilePlan {
    pub template_args: Vec<String>,
    pub csl_path: String,
    pub bibliography_path: String,
    pub resource_path: String,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PaperCompileLogSummary {
    pub summary: String,
    pub log_prefix: String,
    pub truncated: bool,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PubChemCompoundInfo {
    pub status: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub formula: String,
    #[serde(default)]
    pub molecular_weight: f64,
    #[serde(default)]
    pub density: Option<f64>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub(super) struct SealedKernelFileTreeNode {
    pub(super) name: String,
    pub(super) full_name: String,
    pub(super) relative_path: String,
    pub(super) is_folder: bool,
    pub(super) note: Option<SealedKernelFileTreeNote>,
    pub(super) children: Vec<SealedKernelFileTreeNode>,
    pub(super) file_count: u32,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub(super) struct SealedKernelFileTreeNote {
    pub(super) rel_path: String,
    pub(super) name: String,
    pub(super) extension: String,
    pub(super) mtime_ns: u64,
}
