use std::collections::{HashSet, VecDeque};
use std::hash::{Hash, Hasher};
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

fn node_id_from_smiles(smiles: &str) -> String {
    let normalized = normalized_smiles(smiles);
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    normalized.hash(&mut hasher);
    format!("retro_{:x}", hasher.finish())
}

fn fallback_pathway(smiles: &str) -> ReactionPathway {
    let target_id = node_id_from_smiles(smiles);
    ReactionPathway {
        target_id,
        reaction_name: "Generic Bond Disconnection".to_string(),
        conditions: "Base-mediated two-component assembly".to_string(),
        precursors: vec![
            PrecursorNode {
                id: node_id_from_smiles("C1=CC=CC=C1"),
                smiles: "C1=CC=CC=C1".to_string(),
                role: "reactant".to_string(),
            },
            PrecursorNode {
                id: node_id_from_smiles("O=C(O)C"),
                smiles: "O=C(O)C".to_string(),
                role: "reactant".to_string(),
            },
            PrecursorNode {
                id: node_id_from_smiles("K2CO3"),
                smiles: "K2CO3".to_string(),
                role: "reagent".to_string(),
            },
        ],
    }
}

fn infer_mock_pathway(smiles: &str) -> ReactionPathway {
    let normalized = normalized_smiles(smiles);
    let target_id = node_id_from_smiles(&normalized);

    if normalized.contains("C(=O)N") {
        return ReactionPathway {
            target_id,
            reaction_name: "Amide Coupling".to_string(),
            conditions: "EDC·HCl, DIPEA, DMF, rt".to_string(),
            precursors: vec![
                PrecursorNode {
                    id: node_id_from_smiles("O=C(O)C1=CC=CC=C1"),
                    smiles: "O=C(O)C1=CC=CC=C1".to_string(),
                    role: "reactant".to_string(),
                },
                PrecursorNode {
                    id: node_id_from_smiles("NCC1=CC=CC=C1"),
                    smiles: "NCC1=CC=CC=C1".to_string(),
                    role: "reactant".to_string(),
                },
                PrecursorNode {
                    id: node_id_from_smiles("HATU"),
                    smiles: "HATU".to_string(),
                    role: "reagent".to_string(),
                },
            ],
        };
    }

    if normalized.contains("C(=O)O") {
        return ReactionPathway {
            target_id,
            reaction_name: "Fischer Esterification".to_string(),
            conditions: "H2SO4 (cat.), EtOH, reflux".to_string(),
            precursors: vec![
                PrecursorNode {
                    id: node_id_from_smiles("O=C(O)C1=CC=CC=C1"),
                    smiles: "O=C(O)C1=CC=CC=C1".to_string(),
                    role: "reactant".to_string(),
                },
                PrecursorNode {
                    id: node_id_from_smiles("CCO"),
                    smiles: "CCO".to_string(),
                    role: "reactant".to_string(),
                },
            ],
        };
    }

    if normalized.contains("Br") || normalized.contains("I") || normalized.contains("B(") {
        return ReactionPathway {
            target_id,
            reaction_name: "Suzuki-Miyaura Coupling".to_string(),
            conditions: "Pd(PPh3)4, K2CO3, THF, 80°C".to_string(),
            precursors: vec![
                PrecursorNode {
                    id: node_id_from_smiles("B(O)Oc1ccccc1"),
                    smiles: "B(O)Oc1ccccc1".to_string(),
                    role: "reactant".to_string(),
                },
                PrecursorNode {
                    id: node_id_from_smiles("Brc1ccccc1"),
                    smiles: "Brc1ccccc1".to_string(),
                    role: "reactant".to_string(),
                },
                PrecursorNode {
                    id: node_id_from_smiles("[Pd]"),
                    smiles: "[Pd]".to_string(),
                    role: "catalyst".to_string(),
                },
            ],
        };
    }

    fallback_pathway(&normalized)
}

pub async fn retrosynthesize_target(
    target_smiles: String,
    depth: u8,
) -> Result<RetroTreeData, String> {
    let root_smiles = normalized_smiles(&target_smiles);
    if root_smiles.is_empty() {
        return Err("请输入目标分子 SMILES".to_string());
    }

    let max_depth = depth.clamp(1, 4);
    let mut pathways: Vec<ReactionPathway> = Vec::new();
    let mut queue: VecDeque<(String, String, u8)> = VecDeque::new();
    let mut expanded: HashSet<String> = HashSet::new();

    let root_id = node_id_from_smiles(&root_smiles);
    queue.push_back((root_id, root_smiles, 0));

    while let Some((target_id, smiles, level)) = queue.pop_front() {
        if level >= max_depth {
            continue;
        }
        if !expanded.insert(target_id.clone()) {
            continue;
        }

        let mut pathway = infer_mock_pathway(&smiles);
        pathway.target_id = target_id.clone();
        pathways.push(pathway.clone());

        let next_level = level + 1;
        if next_level >= max_depth {
            continue;
        }

        for precursor in pathway.precursors {
            if precursor.role != "reactant" {
                continue;
            }
            queue.push_back((precursor.id, precursor.smiles, next_level));
        }
    }

    if pathways.is_empty() {
        return Err("未生成可用逆合成路径".to_string());
    }

    Ok(RetroTreeData { pathways })
}
