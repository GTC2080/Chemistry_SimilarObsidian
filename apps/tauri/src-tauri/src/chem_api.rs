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

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct PrecursorNode {
    pub id: String,
    pub smiles: String,
    pub role: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ReactionPathway {
    pub target_id: String,
    pub precursors: Vec<PrecursorNode>,
    pub reaction_name: String,
    pub conditions: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct RetroTreeData {
    pub pathways: Vec<ReactionPathway>,
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
    let query = crate::sealed_kernel::normalize_pubchem_query(&query)
        .map_err(|_| "请输入化合物名称".to_string())?;

    let url = build_pubchem_url(&query)?;
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
    let first = properties.first();
    let formula = first
        .and_then(|property| property.molecular_formula.as_deref())
        .unwrap_or("");
    let molecular_weight = first
        .and_then(|property| property.molecular_weight)
        .unwrap_or_default();
    let density = first.and_then(|property| property.density);
    let normalized = crate::sealed_kernel::build_pubchem_compound_info(
        &query,
        formula,
        molecular_weight,
        density,
        properties.len() as u64,
    )
    .map_err(|err| err.to_string())?;

    match normalized.status.as_str() {
        "ok" => Ok(CompoundInfo {
            name: normalized.name,
            formula: normalized.formula,
            molecular_weight: normalized.molecular_weight,
            density: normalized.density,
        }),
        "ambiguous" => Err("匹配结果不唯一，请补充化合物名称".to_string()),
        "emptyQuery" => Err("请输入化合物名称".to_string()),
        _ => Err("未找到该化合物".to_string()),
    }
}

fn retrosynthesize_target_from_kernel(
    target_smiles: String,
    depth: u8,
) -> Result<RetroTreeData, String> {
    crate::sealed_kernel::generate_mock_retrosynthesis(&target_smiles, depth)
        .map_err(|err| err.to_string())
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
    fn retrosynthesis_delegates_empty_smiles_to_kernel() {
        let err = retrosynthesize_target_from_kernel("   ".to_string(), 2).unwrap_err();

        assert_eq!(err, "请输入目标分子 SMILES");
    }
}
