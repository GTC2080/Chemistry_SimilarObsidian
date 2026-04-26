//! 晶格与密勒面命令 DTO。
//!
//! CIF 解析、超晶胞构建、晶胞基矢和密勒面几何均由 C++ sealed kernel
//! bridge 调用 kernel 计算面完成。

mod types;

pub use types::{LatticeData, MillerPlaneData};
