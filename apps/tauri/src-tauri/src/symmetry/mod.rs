//! 分子点群与空间对称性命令 DTO。
//!
//! 解析、形状分析、主轴计算、候选生成、操作匹配、点群分类与渲染几何
//! 均由 C++ sealed kernel bridge 调用 kernel 计算面完成。

mod types;

pub use types::SymmetryData;
