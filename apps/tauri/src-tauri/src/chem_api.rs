use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::slice;
use std::time::Duration;

use reqwest::{Client, StatusCode, Url};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize)]
pub struct CompoundInfo {
    pub name: String,
    pub formula: String,
    pub molecular_weight: f64,
    pub density: Option<f64>,
}

#[derive(Debug, Serialize, Clone)]
pub struct PrecursorNode {
    pub id: String,
    pub smiles: String,
    pub role: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct ReactionPathway {
    pub target_id: String,
    pub precursors: Vec<PrecursorNode>,
    pub reaction_name: String,
    pub conditions: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct RetroTreeData {
    pub pathways: Vec<ReactionPathway>,
}

const KERNEL_OK: i32 = 0;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelRetroPrecursor {
    id: *mut c_char,
    smiles: *mut c_char,
    role: *mut c_char,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelRetroPathway {
    target_id: *mut c_char,
    reaction_name: *mut c_char,
    conditions: *mut c_char,
    precursors: *mut KernelRetroPrecursor,
    precursor_count: usize,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelRetroTree {
    pathways: *mut KernelRetroPathway,
    pathway_count: usize,
}

impl Default for KernelRetroTree {
    fn default() -> Self {
        Self {
            pathways: std::ptr::null_mut(),
            pathway_count: 0,
        }
    }
}

extern "C" {
    fn kernel_generate_mock_retrosynthesis(
        target_smiles: *const c_char,
        depth: u8,
        out_tree: *mut KernelRetroTree,
    ) -> KernelStatus;
    fn kernel_free_retro_tree(tree: *mut KernelRetroTree);
}

#[derive(Debug, Deserialize)]
struct PubChemResponse {
    #[serde(rename = "PropertyTable")]
    property_table: Option<PubChemPropertyTable>,
}

#[derive(Debug, Deserialize)]
struct PubChemPropertyTable {
    #[serde(rename = "Properties")]
    properties: Vec<PubChemProperty>,
}

#[derive(Debug, Deserialize)]
struct PubChemProperty {
    #[serde(rename = "MolecularFormula")]
    molecular_formula: Option<String>,
    #[serde(rename = "MolecularWeight")]
    molecular_weight: Option<f64>,
    #[serde(rename = "Density")]
    density: Option<f64>,
}

fn create_client() -> Result<Client, String> {
    Client::builder()
        .timeout(Duration::from_secs(8))
        .build()
        .map_err(|_| "网络客户端初始化失败".to_string())
}

fn build_pubchem_url(query: &str) -> Result<Url, String> {
    let mut url = Url::parse("https://pubchem.ncbi.nlm.nih.gov/rest/pug/compound/name/")
        .map_err(|_| "服务地址不可用".to_string())?;

    let mut segments = url
        .path_segments_mut()
        .map_err(|_| "服务地址不可用".to_string())?;
    segments.push(query);
    segments.push("property");
    segments.push("MolecularFormula,MolecularWeight,Density");
    segments.push("JSON");
    drop(segments);

    Ok(url)
}

pub async fn fetch_compound_info(query: String) -> Result<CompoundInfo, String> {
    let query = query.trim();
    if query.is_empty() {
        return Err("请输入化合物名称".to_string());
    }

    let url = build_pubchem_url(query)?;
    let client = create_client()?;
    let response = client
        .get(url)
        .send()
        .await
        .map_err(|_| "网络请求失败，请稍后重试".to_string())?;

    match response.status() {
        StatusCode::OK => {}
        StatusCode::NOT_FOUND => return Err("未找到该化合物".to_string()),
        StatusCode::TOO_MANY_REQUESTS => return Err("请求过于频繁，请稍后再试".to_string()),
        _ => return Err("暂时无法获取化合物信息".to_string()),
    }

    let payload = response
        .json::<PubChemResponse>()
        .await
        .map_err(|_| "返回数据解析失败".to_string())?;

    let properties = payload
        .property_table
        .map(|table| table.properties)
        .unwrap_or_default();
    if properties.is_empty() {
        return Err("未找到该化合物".to_string());
    }
    if properties.len() > 1 {
        return Err("匹配结果不唯一，请补充化合物名称".to_string());
    }
    let first = &properties[0];

    let formula = first
        .molecular_formula
        .clone()
        .unwrap_or_default()
        .trim()
        .to_string();
    let molecular_weight = first.molecular_weight.unwrap_or_default();
    let density = first.density.filter(|v| v.is_finite() && *v > 0.0);

    if formula.is_empty() || !molecular_weight.is_finite() || molecular_weight <= 0.0 {
        return Err("未找到该化合物".to_string());
    }

    Ok(CompoundInfo {
        name: query.to_string(),
        formula,
        molecular_weight,
        density,
    })
}

fn normalized_smiles(smiles: &str) -> String {
    smiles.split_whitespace().collect::<String>()
}

unsafe fn kernel_string(ptr: *const c_char, label: &str) -> Result<String, String> {
    if ptr.is_null() {
        return Err(format!("kernel retrosynthesis returned null {}", label));
    }
    CStr::from_ptr(ptr)
        .to_str()
        .map(|value| value.to_string())
        .map_err(|_| format!("kernel retrosynthesis returned invalid UTF-8 {}", label))
}

unsafe fn precursor_from_kernel(raw: &KernelRetroPrecursor) -> Result<PrecursorNode, String> {
    Ok(PrecursorNode {
        id: kernel_string(raw.id, "precursor id")?,
        smiles: kernel_string(raw.smiles, "precursor smiles")?,
        role: kernel_string(raw.role, "precursor role")?,
    })
}

unsafe fn pathway_from_kernel(raw: &KernelRetroPathway) -> Result<ReactionPathway, String> {
    let precursor_slice = if raw.precursor_count == 0 {
        &[][..]
    } else {
        if raw.precursors.is_null() {
            return Err("kernel retrosynthesis returned null precursor list".to_string());
        }
        slice::from_raw_parts(raw.precursors, raw.precursor_count)
    };

    let mut precursors = Vec::with_capacity(precursor_slice.len());
    for precursor in precursor_slice {
        precursors.push(precursor_from_kernel(precursor)?);
    }

    Ok(ReactionPathway {
        target_id: kernel_string(raw.target_id, "target id")?,
        reaction_name: kernel_string(raw.reaction_name, "reaction name")?,
        conditions: kernel_string(raw.conditions, "conditions")?,
        precursors,
    })
}

unsafe fn retro_tree_from_kernel(raw: &KernelRetroTree) -> Result<RetroTreeData, String> {
    let pathway_slice = if raw.pathway_count == 0 {
        &[][..]
    } else {
        if raw.pathways.is_null() {
            return Err("kernel retrosynthesis returned null pathway list".to_string());
        }
        slice::from_raw_parts(raw.pathways, raw.pathway_count)
    };

    let mut pathways = Vec::with_capacity(pathway_slice.len());
    for pathway in pathway_slice {
        pathways.push(pathway_from_kernel(pathway)?);
    }
    if pathways.is_empty() {
        return Err("未生成可用逆合成路径".to_string());
    }

    Ok(RetroTreeData { pathways })
}

fn retrosynthesize_target_from_kernel(
    target_smiles: String,
    depth: u8,
) -> Result<RetroTreeData, String> {
    let root_smiles = normalized_smiles(&target_smiles);
    if root_smiles.is_empty() {
        return Err("请输入目标分子 SMILES".to_string());
    }

    let c_smiles =
        CString::new(root_smiles).map_err(|_| "目标分子 SMILES 包含无效字符".to_string())?;
    let mut raw_tree = KernelRetroTree::default();
    let status =
        unsafe { kernel_generate_mock_retrosynthesis(c_smiles.as_ptr(), depth, &mut raw_tree) };
    if status.code != KERNEL_OK {
        unsafe { kernel_free_retro_tree(&mut raw_tree) };
        return Err("未生成可用逆合成路径".to_string());
    }

    let result = unsafe { retro_tree_from_kernel(&raw_tree) };
    unsafe { kernel_free_retro_tree(&mut raw_tree) };
    result
}

pub async fn retrosynthesize_target(
    target_smiles: String,
    depth: u8,
) -> Result<RetroTreeData, String> {
    retrosynthesize_target_from_kernel(target_smiles, depth)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn retrosynthesis_uses_kernel_amide_rules() {
        let result =
            retrosynthesize_target_from_kernel(" CC(=O)NCC1=CC=CC=C1 ".to_string(), 2).unwrap();

        assert!(!result.pathways.is_empty());
        assert_eq!(result.pathways[0].reaction_name, "Amide Coupling");
        assert!(result.pathways[0].target_id.starts_with("retro_"));
        assert_eq!(result.pathways[0].precursors[2].role, "reagent");
    }

    #[test]
    fn retrosynthesis_rejects_empty_smiles_before_kernel_call() {
        let err = retrosynthesize_target_from_kernel("   ".to_string(), 2).unwrap_err();

        assert_eq!(err, "请输入目标分子 SMILES");
    }
}
