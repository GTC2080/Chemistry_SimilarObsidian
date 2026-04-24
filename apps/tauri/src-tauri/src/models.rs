use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// 笔记信息结构体
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NoteInfo {
    pub id: String,
    pub name: String,
    pub path: String,
    pub created_at: i64,
    pub updated_at: i64,
    pub file_extension: String,
}

/// 图谱节点
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphNode {
    pub id: String,
    pub name: String,
    /// 是否为幽灵节点（被链接但尚未创建的笔记）
    pub ghost: bool,
}

/// 图谱连线
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphLink {
    pub source: String,
    pub target: String,
    /// 连线类型："link" = wikilink, "tag" = 标签共现, "folder" = 同文件夹
    pub kind: String,
}

/// 标签聚合信息
#[derive(Debug, Clone, Serialize)]
pub struct TagInfo {
    pub name: String,
    pub count: u32,
}

/// 全局关系图谱数据
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphData {
    pub nodes: Vec<GraphNode>,
    pub links: Vec<GraphLink>,
}

/// 增强版图谱数据，包含预计算的邻接索引
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct EnrichedGraphData {
    pub nodes: Vec<GraphNode>,
    pub links: Vec<GraphLink>,
    /// 双向邻居索引: nodeId → [neighborId, ...]
    pub neighbors: HashMap<String, Vec<String>>,
    /// 双向链接对集合: ["source->target", ...]
    pub link_pairs: Vec<String>,
}

/// 标签树节点（层级结构）
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TagTreeNode {
    pub name: String,
    pub full_path: String,
    pub count: u32,
    pub children: Vec<TagTreeNode>,
}

/// 文件树节点
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FileTreeNode {
    pub name: String,
    pub full_name: String,
    pub relative_path: String,
    pub is_folder: bool,
    pub note: Option<NoteInfo>,
    pub children: Vec<FileTreeNode>,
    pub file_count: u32,
}

/// 波谱序列（用于多通道导出）
#[derive(Debug, Clone, Serialize)]
pub struct SpectrumSeries {
    pub y: Vec<f64>,
    pub label: String,
}

/// 波谱解析结果
#[derive(Debug, Clone, Serialize)]
pub struct SpectroscopyData {
    pub x: Vec<f64>,
    pub series: Vec<SpectrumSeries>,
    pub x_label: String,
    pub title: String,
    pub is_nmr: bool,
}
