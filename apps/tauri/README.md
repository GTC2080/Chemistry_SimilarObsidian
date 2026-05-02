<p align="center">
  <a href="README_EN.md">English</a> | 简体中文
</p>

<p align="center">
  <img src=".logo/Logo.png" width="120" alt="Nexus Logo" />
</p>

<h1 align="center">Nexus · 星枢</h1>

<p align="center">
  本地优先的智能知识管理工具，类 Obsidian 体验，内置 AI 能力。
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-blue?logo=tauri" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61dafb?logo=react" alt="React 19" />
  <img src="https://img.shields.io/badge/Rust-2021-orange?logo=rust" alt="Rust" />
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT License" />
</p>

---

## 核心亮点

- **本地优先知识库** — Markdown + SQLite + 本地文件系统，支持 `[[双向链接]]`、`#标签`、LaTeX 数学公式与图片/PDF 预览
- **AI 知识工作流** — 语义搜索、语义共鸣、RAG 问答一体化，支持 OpenAI 兼容接口
- **PDF 阅读与批注** — pdf.js 前端渲染（秒开）、文本高亮（5 色）、手绘涂写（压感 + C++ kernel 笔迹平滑）、目录导航、全文搜索、阅读位置记忆、批注持久化与删除
- **化学工作台** — Ketcher 2D 分子编辑、3D 结构查看、点群对称性、晶格解析、波谱可视化、高分子动力学沙盘
- **论文与发刊** — `.paper` 工作台支持拖拽组装内容，并通过 Pandoc + XeLaTeX 生成 PDF
- **关系化浏览** — 文件树、标签树、知识图谱、学习时间轴共同构成”写作 + 复习 + 回溯”闭环
- **桌面端性能优化** — Tauri + Rust 负责重计算与 I/O；PDF 渲染已迁移至前端 pdf.js（零 IPC 渲染），PDFium 已完全移除
- **完整应用体验** — 引导流程、主题系统、中英双语、可调布局、TRUTH_SYSTEM 看板已打通

## 当前状态

- **当前开发目标**：`v1.0.5`
- **产品定位**：当前版本偏化学科研/学习场景，但 Markdown、知识图谱、搜索和 AI 能力本身是通用的
- **数据策略**：默认完全本地，知识库和数据库都留在用户机器上
- **性能方向**：持续追求大库可用性、交互即时反馈、低阻塞 I/O 和更稳的语义检索链路
- **后端边界**：核心笔记读写、搜索、标签、反链、标签树与知识图谱读面以 `kernel/` C++ sealed kernel 为准；Tauri Rust 只负责命令注册、桥接、平台能力和仍在迁移中的薄壳服务

## 最近更新

- **v1.0.6-dev** — vault host path 边界继续内核化：`read_note_by_file_path` / `write_note_by_file_path` / create/delete/rename/move by-path entry commands 和 watcher notify 事件的绝对宿主路径到 vault 相对路径转换改走 `kernel_relativize_vault_path(...)`；Rust 不再用 `strip_prefix`、反斜杠替换或 `..` 检查持有 vault-root 归属规则。
- **v1.0.6-dev** — 产品计算面继续内核化：`compute_truth_diff` 改为通过 sealed C++ bridge 调用 `kernel_compute_truth_diff(...)`，`build_semantic_context` 改为通过 sealed C++ bridge 调用 `kernel_build_semantic_context(...)`；TRUTH_SYSTEM 经验归因、reason key、代码块语言增量、分子编辑行数规则和 AI 语义上下文裁剪由 C++ sealed kernel 统一提供。Tauri Rust 只保留 DTO 映射和中文 reason 文案，不再保留 product compute C ABI mirror / unsafe 拷贝循环或 truth award reason 整数镜像。
- **v1.0.6-dev** — vault 相对路径校验继续内核化：`read_note` / `write_note` / chemistry reference commands 的 rel path trim、反斜杠归一、NUL/root/drive/`.`/`..`/空 segment 拒绝规则改走 `kernel_normalize_vault_relative_path(...)`；Tauri Rust 只保留中文错误外壳，不再在 `sealed_kernel.rs` 里重建路径真相。
- **v1.0.6-dev** — AI/product 文本边界继续内核化：`get_related_notes_raw` 的 semantic context 最小字节阈值通过 `kernel_get_semantic_context_min_bytes(...)` 提供；embedding 输入截断与空文本判断由 `kernel_normalize_ai_embedding_text(...)` 在 kernel 内部应用；RAG 单笔记截断长度由 `kernel_build_ai_rag_context(...)` 在 kernel 内部应用；Rust 只保留网络请求、缓存和 IPC 编排。
- **v1.0.6-dev** — AI host 运行默认值继续内核化：chat/ponder/embedding request timeout、embedding cache limit、embedding concurrency limit 和 RAG top-note limit 由 `kernel_get_ai_*` getter 提供；Rust 只按 kernel 默认值配置 reqwest、缓存淘汰、semaphore 和 `ask_vault` 的向量检索候选数。
- **v1.0.6-dev** — AI embedding cache key 继续内核化：base URL / embedding model / normalized text 的 stable 16-hex key 由 `kernel_compute_ai_embedding_cache_key(...)` 提供；Rust 不再使用 `DefaultHasher` 持有 key 规则。
- **v1.0.6-dev** — AI prompt shape 继续内核化：RAG system content、Ponder system prompt、Ponder user prompt shape 和 Ponder temperature 由 product-compute kernel ABI 提供；Rust AI 模块只负责 HTTP 请求、stream 解析和结果映射。
- **v1.0.6-dev** — AI embedding 输入归一化继续内核化：embedding request input 的 2000 Unicode 字符截断、保留原文本形状和空白文本跳过规则由 `kernel_normalize_ai_embedding_text(...)` 提供；Rust `ai/embedding.rs` 只负责缓存 key、HTTP 请求、并发许可和错误映射。
- **v1.0.6-dev** — AI RAG context shape 继续内核化：RAG note header、1-based 编号、note 分隔符和每条 note 的 Unicode 字符截断由 `kernel_build_ai_rag_context(...)` 提供；Rust `build_rag_context` 只调用 sealed bridge，不再拼接 note 上下文格式。
- **v1.0.6-dev** — AI RAG note display name 继续内核化：`ask_vault` 只把 kernel rel path 与正文交给 `kernel_build_ai_rag_context_from_note_paths(...)`；最终路径段、扩展名剥离和 RAG block 展示名由 kernel 统一派生。
- **v1.0.6-dev** — media 文件扩展名派生继续内核化：`parse_spectroscopy` / `read_molecular_preview` 只把原始 file path 交给 `kernel_derive_file_extension_from_path(...)`；最终路径段识别、大小写归一和父目录点号忽略由 kernel 统一提供。
- **v1.0.6-dev** — legacy embedding cache 元数据继续瘦身：`db/common.rs` 的文件扩展名派生改走 `kernel_derive_file_extension_from_path(...)`，Rust 不再用 `Path::extension().to_lowercase()` 保留一套扩展名规则。
- **v1.0.6-dev** — note catalog DTO 映射继续瘦身：`sealed_kernel.rs` 将 kernel note record 转为前端 `NoteInfo` 时，`file_extension` 也改走 `kernel_derive_file_extension_from_path(...)`，Rust 不再在 bridge 层保留 `Path::extension().to_lowercase()` 语义。
- **v1.0.6-dev** — note catalog display name 继续内核化：`sealed_kernel.rs` 将 kernel note record 转为前端 `NoteInfo` 时，`name` 改走 `kernel_derive_note_display_name_from_path(...)`；最终路径段、扩展名剥离、无扩展名与 dotfile 展示名规则由 kernel 统一提供。
- **v1.0.6-dev** — tag tree 结构继续内核化：`get_tag_tree` 改走 `kernel_query_tag_tree(...)`，slash-separated tag hierarchy、`fullPath`、exact tag count 与 synthetic parent count 由 kernel 统一派生；Rust 不再用 `split('/')` 现场拼标签树。
- **v1.0.6-dev** — database grid 列类型归一化继续内核化：`normalize_database` 的 `text` / `number` / `select` / `tags` 白名单与未知类型回退规则改走 `kernel_normalize_database_column_type(...)`；Rust `cmd_compute.rs` 不再保留本地列类型 allow-list。
- **v1.0.6-dev** — study truth 经验状态继续内核化：`truth_state_from_study` 只在 Rust 聚合 SQLite activity rows，扩展名属性路由、秒数转 EXP、总等级曲线和属性等级曲线由 `kernel_compute_truth_state_from_activity(...)` 提供。
- **v1.0.6-dev** — study stats 聚合窗口继续内核化：`query_stats` 只用 Rust 查询 SQLite，today/week/daily/legacy heatmap 的 UTC 窗口边界、today bucket 和 folder ranking limit 由 `kernel_compute_study_stats_window(...)` 提供。
- **v1.0.6-dev** — study heatmap 网格继续内核化：`get_heatmap_cells` 只在 Rust 读取 SQLite daily activity rows，26x7 网格大小、UTC 日期格式化、周一对齐、cell 坐标和 max_secs 由 `kernel_build_study_heatmap_grid(...)` 提供。
- **v1.0.6-dev** — study streak 连续天数规则继续内核化：Rust 只从 SQLite 读取 active session timestamp，day bucket 计算、重复 bucket 去重、从今天向前连续计数和缺失当天归零规则由 `kernel_compute_study_streak_days_from_timestamps(...)` 提供。
- **v1.0.6-dev** — AI RAG 笔记读取继续瘦身：`ask_vault` 的候选 note id 先交给 `kernel_filter_changed_markdown_paths(...)` 做 Markdown 过滤、路径归一和去重，Rust 只负责按 kernel rel path 读取正文并拼接上下文。
- **v1.0.6-dev** — 内容/文件工作流继续收口到 C++ sealed kernel：`scan_vault`、`scan_changed_entries`、`index_vault_content`、`index_changed_entries` 的笔记元数据统一来自 kernel note catalog，正文与 media 文本读取走 kernel note read surface；全量 note catalog 与 `build_file_tree` 的 ignored root 过滤分别由 `kernel_query_notes_filtered(...)`、`kernel_query_file_tree_filtered(...)` 提供，增量 changed-entry 的 Markdown 路径归一/过滤/去重由 `kernel_filter_changed_markdown_paths(...)` 提供。Tauri Rust 只保留命令编排、embedding 兼容缓存写入和后台任务调度。
- **v1.0.6-dev** — note catalog 扫描默认上限改由 `kernel_get_note_catalog_default_limit(...)` 提供，`scan_vault` 快速元数据预览上限改由 `kernel_get_vault_scan_default_limit(...)` 提供；`scan_vault` / `index_vault_content` 不再在 Rust command 中保留重复的 catalog limit 常量。
- **v1.0.6-dev** — 直接 note catalog 查询的宿主默认页大小改由 `kernel_get_note_query_default_limit(...)` 提供；`sealed_kernel_query_notes` 不再用 Rust `unwrap_or(512)` 持有查询默认值。
- **v1.0.6-dev** — file tree 默认 source catalog 上限改由 `kernel_get_file_tree_default_limit(...)` 提供；`build_file_tree` 不再在 Rust command 中保留重复的 file-tree limit 常量。
- **v1.0.6-dev** — 关系读面收口到 C++ sealed kernel：`search_notes`、`get_backlinks`、`get_all_tags`、`get_notes_by_tag`、`get_tag_tree`、`get_graph_data`、`get_enriched_graph_data` 通过 `src-tauri/native/sealed_kernel_bridge.*` 调用 `kernel_query_*` / `kernel_search_*` 出口。前端继续只消费 Tauri command，不直接构造 tags / backlinks / graph 的真相结构。
- **v1.0.6-dev** — 关系读面默认上限继续内核化：search/backlinks/tags/tag tree/graph 的默认 limit 通过 `kernel_get_*_default_limit(...)` 查询；Rust `cmd_search.rs` 不再保留 `10` / `64` / `128` / `512` / `2048` 关系查询边界常量。
- **v1.0.6-dev** — chemistry spectrum 读面默认上限继续内核化：spectrum catalog、note -> spectrum refs、spectrum -> referrers 的默认 limit 通过 chemistry kernel getter 查询；`sealed_kernel.rs` 不再用 Rust `unwrap_or(512)` 持有这组读面边界。
- **v1.0.6-dev** — 化学无状态计算继续内核化：高分子动力学、化学计量、波谱解析、分子预览、逆合成 mock pathway 规则均通过 `kernel/` C ABI 提供；kinetics / stoichiometry / retrosynthesis / spectroscopy / molecular preview 的 kernel 结果由 sealed C++ bridge 序列化为 JSON。Tauri Rust 只保留 PubChem HTTP 查询、命令 DTO 映射和 localized error 映射，不再保留这些计算面的 C ABI mirror / unsafe 拷贝循环。
- **v1.0.6-dev** — 对称性与晶体计算上限继续收口：symmetry command atom limit 通过 `kernel_get_symmetry_atom_limit(...)` 查询，crystal supercell atom cap 通过 `kernel_get_crystal_supercell_atom_limit(...)` 查询；Tauri Rust 不再保留重复的 `500` / `50000` 业务边界常量。
- **v1.0.6-dev** — 晶体计算 full-result 化：`parse_and_build_lattice` 通过 sealed C++ bridge 调用 `kernel_build_lattice_from_cif(...)` 一次性取得 CIF 解析、晶胞基矢、超晶胞原子；`calculate_miller_plane` 通过 sealed C++ bridge 调用 `kernel_calculate_miller_plane_from_cif(...)` 完成 CIF 解析与密勒面计算。Rust `crystal/` 只保留最终 DTO，不再保留晶格/密勒面 C ABI mirror 与 unsafe 拷贝循环。
- **v1.0.6-dev** — 对称性计算管线继续内核化：`calculate_symmetry` 现在通过 sealed C++ bridge 单点调用 `kernel_calculate_symmetry(...)`，由 kernel 完成 `XYZ` / `PDB` / simple `CIF` 原子解析、形状分析、主轴计算、候选生成、操作匹配、点群分类和渲染几何；Rust 只保留命令 DTO 与 localized error 映射，不再保留对称性 C ABI mirror / unsafe 拷贝循环。
- **v1.0.6-dev** — PDF ink smoothing 继续瘦身：`smooth_ink_strokes` 通过 sealed C++ bridge 调用 `kernel_smooth_ink_strokes(...)`，默认容差通过 `kernel_get_pdf_ink_default_tolerance(...)` 查询，由 bridge 序列化 kernel-owned strokes/points 为 JSON。Rust `pdf/ink.rs` 只保留前端 DTO 与薄 wrapper，不再保留 ink C ABI mirror / unsafe 拷贝循环。
- **v1.0.6-dev** — PDF annotation 哈希与路径边界继续内核化：批注存储 key 通过 `kernel_compute_pdf_annotation_storage_key(...)` 生成，轻量 PDF 内容 hash 通过 `kernel_compute_pdf_lightweight_hash(...)` 生成；`load_pdf_annotations` / `save_pdf_annotations` 先用 `kernel_relativize_vault_path(...)` 把 viewer 绝对路径转成 vault-relative `pdfPath`，Rust `pdf/annotations.rs` 只保留文件 seek/read、JSON 持久化和 DTO。
- **v1.0.5** — PDF 渲染引擎迁移：PDFium → pdf.js（零 IPC 渲染，秒开）；新增 PDF 手绘/涂写批注（Douglas-Peucker + Catmull-Rom 笔迹平滑）、批注删除、目录提取；移除 pdfium-render/webp/base64 三个 crate 依赖，二进制更小编译更快。15 项性能优化、`VectorCacheState` top-k 修复、晶格解析器；PDF Viewer 模块化拆分（847 行 → 128 行渲染 + 4 个子 hook + 7 个 CSS 子文件）
- **v1.0.4** — 大量前端重计算下沉到 Rust，减少前端热路径计算，优化启动、切换和统计面板响应

## 技术栈

| 层级 | 技术 |
|------|------|
| 框架 | Tauri 2 |
| 前端 | React 19 + TypeScript + Tailwind CSS 4 |
| 编辑器 | TipTap 3 + KaTeX + 3Dmol.js + Ketcher |
| PDF | pdf.js 4 (前端渲染) + C++ kernel 笔迹平滑 |
| 后端 | Tauri Rust 壳层 + C++ sealed kernel + SQLite |
| AI | OpenAI 兼容 API (Chat + Embedding) |
| 构建 | Vite 6 |

## Kernel 接线边界

当前 Tauri 宿主通过 `src-tauri/native/sealed_kernel_bridge.cpp` 嵌入并调用 `kernel/` C ABI。

已收口到 kernel 的内容/文件工作流读写面：

- `scan_vault` / `scan_changed_entries` -> `kernel_query_notes(...)` / `kernel_query_notes_filtered(...)`
- `build_file_tree` -> `kernel_query_file_tree_filtered(...)`
- watcher host path relativization -> `kernel_relativize_vault_path(...)`
- watcher hidden/ignored/supported path filtering -> `kernel_filter_supported_vault_paths_filtered(...)`
- changed-entry path normalization -> `kernel_filter_changed_markdown_paths(...)`
- one-off vault relative path normalization -> `kernel_normalize_vault_relative_path(...)`
- by-path host path relativization -> `kernel_relativize_vault_path(...)`
- `index_vault_content` / `index_changed_entries` -> `kernel_query_notes(...)` + `kernel_read_note(...)`
- `read_note` / `read_note_indexed_content` -> `kernel_filter_changed_markdown_paths(...)` + `kernel_read_note(...)`
- `ask_vault` RAG note content -> `kernel_filter_changed_markdown_paths(...)` + `kernel_read_note(...)`
- `remove_deleted_entries` -> `kernel_filter_changed_markdown_paths(...)` + legacy embedding cache delete
- `parse_spectroscopy` / `read_molecular_preview` -> `kernel_read_note(...)` + `kernel_derive_file_extension_from_path(...)` + sealed C++ bridge over chemistry kernel compute ABI
- `write_note` -> `kernel_write_note(...)`
- `load_pdf_annotations` / `save_pdf_annotations` -> `kernel_relativize_vault_path(...)` + `kernel_compute_pdf_annotation_storage_key(...)` + `kernel_compute_pdf_lightweight_hash(...)`

Rust `cmd_vault.rs` 当前只负责 Tauri command 编排、AI embedding 兼容缓存和后台任务调度，不再为 changed-entry 路径用 Rust 文件系统 metadata 重建 `NoteInfo`。
`scan_vault` 的快速元数据预览上限与 `index_vault_content` 的默认 note catalog 拉取上限从 kernel 查询，不再由 Rust command 持有业务边界常量。
`sealed_kernel_query_notes` 的直接查询默认页大小从 kernel 查询，不影响 scan/index 使用的全量 catalog 默认上限。
`build_file_tree` 的默认 source catalog 上限从 kernel 查询，不再由 Rust command 持有 file-tree limit 常量。
Legacy embedding cache 里的 `NoteInfo.file_extension` 元数据也通过 `kernel_derive_file_extension_from_path(...)` 派生，避免 `db/common.rs` 保留本地路径/大小写规则。
Kernel note catalog 映射到前端 `NoteInfo` 时同样通过 `kernel_derive_file_extension_from_path(...)` 派生 `file_extension`，避免 sealed bridge 层重新拥有扩展名大小写语义。
Kernel note catalog 映射到前端 `NoteInfo` 时也通过 `kernel_derive_note_display_name_from_path(...)` 派生 `name`，避免 sealed bridge 层重新拥有路径展示名规则。
Rust watcher 只保留 notify 事件分类、平台目录事件判断、原始 ignored-root CSV 透传和 IPC 发送；notify 绝对路径到 vault-relative path 的相对化由 `kernel_relativize_vault_path(...)` 判定，隐藏路径、ignored root 解析/归一、支持扩展名、路径归一化与去重规则由 kernel path filter 统一判定。
`read_note` / `write_note` / chemistry reference commands 的单路径校验与标准化由 `kernel_normalize_vault_relative_path(...)` 统一判定，Rust 只把 kernel 返回的 rel path 继续交给 read/write/reference bridge。
`read_note_by_file_path` / `write_note_by_file_path` / create/delete/rename/move by-path entry commands 的绝对宿主路径归属、vault-root 相对化、分隔符标准化和 root-folder 空路径许可由 `kernel_relativize_vault_path(...)` 统一判定，Rust 只保留中文错误外壳和命令编排。
PDF annotation load/save 的绝对 viewer file path 同样先由 `kernel_relativize_vault_path(...)` 判定 vault 归属；annotation JSON 的 `pdfPath` 与 storage key 使用同一个 vault-relative path，绝对路径只用于实际 PDF 文件 I/O。
Embedding 刷新以 kernel note catalog 的 Markdown note surface 为准，不再在 Rust 侧维护额外的 embeddable extension 白名单。

已收口到 kernel 的关系读面：

- `search_notes` -> `kernel_search_notes_limited(...)`
- `get_backlinks` -> `kernel_query_backlinks(...)`
- `get_all_tags` -> `kernel_query_tags(...)`
- `get_notes_by_tag` -> `kernel_query_tag_notes(...)`
- `get_tag_tree` -> `kernel_query_tag_tree(...)`
- `get_graph_data` / `get_enriched_graph_data` -> `kernel_query_graph(...)`

`get_tag_tree` 的树形结构由 kernel 根据 `kernel_query_tags(...)` 同源 tag catalog 派生；Tauri bridge 只序列化 kernel-owned tree，Rust 不再 split tag path 或维护第二套树形规则。
关系真相必须保持在 kernel：前端只调用 Tauri command，Rust command 只做参数校验、JSON 桥接和 UI 形状适配。
关系读面的默认 limit 也必须从 kernel 查询，不在 Rust command 中写死。

已收口到 kernel 的化学无状态计算面：

- `simulate_polymerization` -> `kernel_simulate_polymerization_kinetics(...)`
- `recalculate_stoichiometry` -> `kernel_recalculate_stoichiometry(...)`
- `parse_spectroscopy` -> `kernel_parse_spectroscopy_text(...)`
- `read_molecular_preview` -> `kernel_build_molecular_preview(...)`
- `retrosynthesize_target` -> `kernel_generate_mock_retrosynthesis(...)`
- `parse_and_build_lattice` -> `kernel_build_lattice_from_cif(...)`
- `calculate_miller_plane` -> `kernel_calculate_miller_plane_from_cif(...)`

`simulate_polymerization` 的 time / conversion / Mn / PDI arrays 由 sealed C++ bridge 从 `kernel_simulate_polymerization_kinetics(...)` 结果序列化为 JSON；Rust `kinetics.rs` 只保留 DTO 与薄命令 wrapper，不再持有 `kernel_polymerization_kinetics_result` mirror 或 result-copy loop。

`parse_spectroscopy` 与 `read_molecular_preview` 的 file path 扩展名派生、扩展名支持范围、CSV/JDX 解析、PDB/XYZ/CIF 预览构造和 atom limit 归一化由 kernel compute ABI 判定；sealed C++ bridge 负责 kernel result -> JSON/text，Tauri Rust 只负责通过 `kernel_read_note(...)` 读取文本、调度阻塞计算和映射中文命令错误。

`calculate_symmetry` 的默认 atom limit 与 `parse_and_build_lattice` 的 supercell atom cap 也从 kernel 查询；Rust 只把上限传给 kernel 计算或用于本地化错误文案，不再持有这些业务边界常量。

`recalculate_stoichiometry` 的空表行为、参考行选择、数值归一和依赖行传播由 kernel 判定；sealed C++ bridge 负责构造 kernel row buffer 并把输出转成 JSON，Tauri Rust 只保留表格 DTO 的字段映射。

`fetch_compound_info` 仍是 Tauri Rust 的网络适配层；逆合成目标归一、空目标校验和 pathway 规则不再由 Rust 构造。
`retrosynthesize_target` 的 pathway tree 由 sealed C++ bridge 从 `kernel_generate_mock_retrosynthesis(...)` 结果序列化，Rust `chem_api.rs` 不再持有 `kernel_retro_tree` mirror 或 kernel-owned string copy loop。

`sealed_kernel_query_chem_spectra` 与 `sealed_kernel_get_chem_spectrum` 直接暴露 kernel chemistry spectrum catalog / lookup；Tauri Rust 不再推断 spectrum carrier、source format、domain object key 或 subtype state。

`sealed_kernel_query_note_chem_spectrum_refs` 与 `sealed_kernel_query_chem_spectrum_referrers` 直接暴露 kernel note <-> spectrum source-reference 面；Tauri Rust 不再从 note text、attachment refs、backlinks 或 search 结果重建 chemistry spectrum 引用关系。
这三条 chemistry spectrum 读面的默认 limit 也从 kernel 查询，不在 Rust command 中写死。

已收口到 kernel 的产品无状态计算面：

- `compute_truth_diff` -> `kernel_compute_truth_diff(...)`
- `build_semantic_context` -> `kernel_build_semantic_context(...)`
- `normalize_embedding_text` -> `kernel_normalize_ai_embedding_text(...)`
- `embedding_cache_key` -> `kernel_compute_ai_embedding_cache_key(...)`
- `build_rag_context` -> `kernel_build_ai_rag_context(...)`
- `build_rag_context_from_note_paths` -> `kernel_build_ai_rag_context_from_note_paths(...)`
- `normalize_database` column type rules -> `kernel_normalize_database_column_type(...)`

TRUTH_SYSTEM 经验归因规则、truth award reason key、AI 语义上下文裁剪规则、embedding 输入归一化规则、embedding cache key 规则、RAG note context shape 和 database grid 列类型归一化由 kernel 提供；sealed C++ bridge 负责释放 kernel-owned truth diff result / semantic/embedding/RAG context buffer，并把结果转成 JSON/text；Rust command 只映射 DTO 和中文展示文案。
AI/product 文本边界默认值、embedding 输入归一化、embedding cache key 和 RAG 截断规则由 kernel 提供；Rust AI 模块只负责 HTTP 客户端、stream 解析、embedding 缓存容器和 IPC 发送，不再持有 semantic context/RAG/embedding 输入长度或 cache-key 真相。
AI host 运行默认值由 kernel 提供；Rust AI 模块不再持有 chat/ponder/embedding timeout、cache limit、concurrency limit 或 RAG top-note limit 真相。
AI prompt shape 和 Ponder temperature 由 kernel 提供；Rust AI 模块不再持有 RAG/Ponder prompt 文案、Ponder 用户提示模板或 temperature 真相。
RAG note context shape 与 note display name derivation 由 kernel 提供；Rust AI 模块不再持有 note header、编号、分隔符、展示名派生或每条 note 的截断规则。
Study truth 经验状态规则由 kernel 提供；Rust study DB 代码只聚合 SQLite activity rows 并补最后结算时间戳。
Study stats 聚合窗口、folder ranking limit、streak 与 heatmap 网格规则由 kernel 提供；Rust study DB 代码只读取 SQLite rows 并映射 DTO。
Tauri Rust 不再保留 product compute C ABI mirror 或 unsafe result-copy loop。

已收口到 kernel 的对称性计算面：

- `calculate_symmetry` 全流程 -> `kernel_calculate_symmetry(...)`
- granular symmetry ABI 仍保留在 kernel 内部回归面：`kernel_parse_symmetry_atoms_text(...)`、`kernel_analyze_symmetry_shape(...)`、`kernel_compute_symmetry_principal_axes(...)`、`kernel_generate_symmetry_candidate_directions(...)` / `kernel_generate_symmetry_candidate_planes(...)`、`kernel_find_symmetry_rotation_axes(...)` / `kernel_find_symmetry_mirror_planes(...)`、`kernel_classify_point_group(...)`、`kernel_build_symmetry_render_geometry(...)`

Rust `symmetry/` 当前只保留 3D viewer DTO；sealed C++ bridge 负责 kernel full-result 计算结果到 JSON 的序列化，Rust `sealed_kernel.rs` 只映射本地化错误并反序列化命令 DTO。

## 快速开始

### 环境要求

- [Node.js](https://nodejs.org/) >= 18
- [Rust](https://rustup.rs/) >= 1.77
- [Tauri 2 CLI 前置依赖](https://v2.tauri.app/start/prerequisites/)

### 安装 & 运行

```bash
# 克隆仓库
git clone https://github.com/GTC2080/Nexus.git
cd Nexus

# 安装前端依赖
npm install

# 开发模式启动（自动编译 Rust + 启动前端）
npx tauri dev

# 构建生产包
npx tauri build
```

## AI 配置

在应用内点击左下角「设置」(⌘,)，填入：

- **Chat 模型** — API Key、Base URL、模型名称（默认 `gpt-4o-mini`）
- **Embedding 模型** — 可选，留空则复用 Chat 配置（默认 `text-embedding-3-small`）

支持任何 OpenAI 兼容的 API 端点。

## 波谱数据支持

支持直接打开科学仪器导出的光谱/波谱数据文件：

| 格式 | 说明 |
|------|------|
| `.csv` | 逗号/制表符分隔的波谱数据（支持 UTF-8 和 UTF-16 LE 编码） |
| `.jdx` | JCAMP-DX 标准格式（化学界通用交换格式） |

- 自动识别多列数据（如多次扫描），每列渲染为独立曲线
- NMR 数据自动检测并反转 x 轴（化学位移从高场到低场）
- 波谱文件不参与数据库内容索引和 Embedding 向量化，避免海量浮点数造成 Token 浪费

## 3D 分子结构支持

应用当前为化学专注模式，可直接打开以下三维结构文件：

| 格式 | 说明 |
|------|------|
| `.pdb` | Protein Data Bank 蛋白质/小分子结构 |
| `.xyz` | XYZ 坐标系格式（计算化学常用） |
| `.cif` | Crystallographic Information File 晶体学数据 |

- 小分子（≤500 原子）默认 ball+stick 渲染，蛋白质自动切换 cartoon 模式
- 深色融合背景，Jmol 科学标准原子配色
- `.cif` 文件支持「结构 / 对称性 / 晶格」三视图：晶格视图提供超晶胞扩展控制（1-5×）与密勒指数切割器，非 CIF 文件保持「结构 / 对称性」双视图
- 在对称性视图中显示点群 HUD、旋转轴与镜像平面（可独立开关）
- 对称性原子解析和点群分类由 C++ kernel 提供：支持 PDB / XYZ / CIF 输入，CIF 晶胞参数支持“标签同一行”与“值在下一行”两种写法
- 分子文件不参与数据库内容索引和 Embedding 向量化，防止海量坐标数据污染语义检索

## 高分子动力学沙盘

化学模式下，在 Markdown 编辑视图点击 `POLYMER KINETICS` 按钮可打开全屏暗色沙盘：

- 左侧通过滑块和数值输入调节参数：`[M]0`、`[I]0`、`[CTA]0`、`kd`、`kp`、`kt`、`ktr`、`timeMax`、`steps`
- 前端使用 `150ms` 防抖触发 IPC，避免滑块拖动造成调用拥塞
- Rust 后端追踪自由基、单体与 0/1/2 阶矩，返回 `time / conversion / Mn / PDI`
- 图表区显示两张实时曲线：
  - `Conversion vs Time`
  - `Mn / PDI vs Conversion`（`PDI` 使用右侧 y 轴）
- 初始阶段自动阻断除零异常：链尚未形成时强制 `Mn = 0`、`PDI = 1.0`

## 项目结构

```
src/                    # React 前端
├── assets/             # 静态资源（Logo / 图标）
├── components/         # UI 组件
│   ├── app/            # 工作区壳层与视口编排（Shell / Runtime / Viewport / LaunchSplash / Modals）
│   ├── ai/             # AI 助手子组件（ChatBubble / AIContextPanel）
│   ├── KineticsSimulator.tsx  # 高分子动力学沙盘（化学模式）
│   ├── CrystalViewer3D.tsx   # 晶格 3D 渲染器（超晶胞 + 密勒面切割，化学模式）
│   ├── AIAssistantSidebar.tsx # AI 助手侧边栏（编排层，逻辑下沉到 hooks）
│   ├── MarkdownEditor.tsx     # 主 Markdown 编辑器（编排层，扩展配置下沉到 hook）
│   ├── onboarding/     # 首次启动引导向导
│   ├── study-timeline/ # 自动学习时间轴面板（热力图/统计/每日记录）
│   ├── chem-editor/    # Ketcher 化学绘图板组件
│   ├── editor/         # 编辑器相关界面组件
│   ├── global-graph/   # 全局知识图谱视图
│   ├── markdown-editor/ # Markdown 编辑器菜单、BubbleMenuBar、上下文操作与辅助工具
│   ├── pdf-viewer/    # PDF 阅读器（模块化拆分）
│   │   ├── PdfViewer.tsx          # 纯渲染壳层（128 行）
│   │   ├── usePdfViewerState.ts   # 状态组合层（组装 4 个子 hook）
│   │   ├── hooks/                 # 按职责拆分的子 hook
│   │   │   ├── useViewerNav.ts    # 导航/缩放/滚动/IO 观察
│   │   │   ├── useAnnotations.ts  # 批注 CRUD/选区工具栏
│   │   │   ├── useDrawing.ts      # 绘图模式/笔画平滑
│   │   │   └── usePdfOutline.ts   # 目录加载
│   │   ├── PdfDrawingLayer.tsx    # Canvas 手绘叠层
│   │   ├── PdfAnnotationLayer.tsx # 高亮/ink/区域渲染
│   │   ├── PdfDrawingToolbar.tsx  # 画笔工具栏
│   │   └── styles/                # 按组件拆分的 CSS（7 个文件）
│   ├── media-viewer/   # 图片/波谱预览组件
│   ├── publish-studio/ # 论文/笔记装配与发布工作台
│   ├── search/         # 搜索结果与语义检索 UI
│   ├── settings/       # 设置面板（按职责拆分为 5 个独立面板：常规/功能/编辑器/AI/知识库 + 共享组件）
│   └── sidebar/        # 侧边栏文件树/标签树/工具入口
├── i18n/               # 国际化（i18n）
│   ├── zh-CN.ts        # 中文翻译字典
│   ├── en.ts           # 英文翻译字典
│   ├── context.tsx     # LanguageProvider / useT / useLanguage
│   └── types.ts        # 语言类型定义
├── editor/             # TipTap 编辑器扩展
│   └── extensions/     # WikiLink / Tag / Math / ChemDraw
├── hooks/              # React Hooks
│   ├── useVaultSession.ts           # 会话编排入口（组合索引、内容、保存与预览 hooks）
│   ├── useVaultIndex.ts             # 知识库扫描、重建索引与活动文件校正
│   ├── useActiveNoteContent.ts      # 当前笔记内容、预览与学科视图状态
│   ├── useAIChatStream.ts           # AI 聊天流式逻辑（消息历史/流式渲染/IPC 通道）
│   ├── useMarkdownEditorExtensions.ts # TipTap 扩展配置（memoized）
│   ├── useSettingsModal.ts          # 设置弹窗状态管理与操作逻辑
│   ├── useFileTreeDragDrop.ts       # 文件树拖拽逻辑
│   ├── useInlineRename.ts           # 行内重命名逻辑
│   ├── useSidebarTags.ts            # 标签面板加载与筛选逻辑
│   ├── useSemanticResonance.ts      # 语义共鸣上下文提取、缓存与自适应防抖
│   ├── useNotePersistence.ts        # 保存去重、排队写盘与 flush 控制
│   ├── useStudyTracker.ts           # 自动学习计时 Hook（活跃检测 + Tauri IPC）
│   ├── useRuntimeSettings.ts        # 设置读取与保存
│   ├── useTruthSystem.ts            # TRUTH_SYSTEM 看板数据与交互
│   └── ...                          # 其余性能与交互 hooks
├── models/             # 前端领域模型
├── types/              # 拆分类型定义
├── *.test.ts           # Vitest 单元测试入口（类型、设置、语义推荐等）
├── utils/              # 工具函数（解析/格式化/通用算法）
└── types.ts            # 历史兼容类型入口

src-tauri/src/          # Rust 后端
├── commands/           # Tauri 命令分模块
│   ├── cmd_vault.rs    # 知识库生命周期与文件操作命令
│   ├── cmd_tree.rs     # 文件树/标签树构建与查询命令
│   ├── cmd_search.rs   # 搜索/FTS/语义检索命令
│   ├── cmd_ai.rs       # AI 问答与推理命令
│   ├── cmd_study.rs    # 学习时间轴记录与统计命令
│   ├── cmd_compute.rs  # TRUTH diff 等计算命令
│   ├── cmd_media.rs    # 媒体与波谱解析命令
│   ├── cmd_pdf.rs      # PDF 命令（文件读取/笔迹平滑/批注持久化）
│   ├── cmd_symmetry.rs # 分子对称性分析命令（桥接 kernel 点群/轴/镜面）
│   └── cmd_crystal.rs  # 晶格解析与密勒面计算命令（桥接 kernel 计算面）
├── commands.rs         # 命令注册入口
├── crystal/            # 晶格与密勒面命令 DTO
│   ├── mod.rs          # DTO re-export（full-result 计算经 sealed kernel bridge）
│   └── types.rs        # 晶格数据协议（LatticeData / UnitCellBox / AtomNode / MillerPlaneData）
├── pdf/                # PDF 模块（渲染已迁移至前端 pdf.js）
│   ├── mod.rs          # 模块入口
│   ├── annotations.rs  # 批注数据结构与 JSON 持久化
│   └── ink.rs          # 笔迹平滑 DTO / sealed kernel bridge
├── kinetics.rs         # 高分子动力学 kernel bridge
├── db.rs               # SQLite 数据库管理
├── db/                 # 数据库子模块
│   ├── schema.rs       # 表结构与迁移
│   ├── embeddings.rs   # 向量索引、Embedding 存储与 embedding cache metadata
│   ├── study.rs        # 学习模块入口（子模块: session / stats / truth）
│   ├── study/          # 学习模块子目录
│   │   ├── session.rs  # 会话 CRUD（start / tick / end）
│   │   ├── stats.rs    # 统计聚合查询（热力图/每日/排行）
│   │   └── truth.rs    # TruthState 经验等级推导
│   ├── lifecycle.rs    # 初始化/清理/维护流程
│   └── common.rs       # DB 公共工具
├── shared/             # 公共 helper 与跨模块共享逻辑
├── services/           # 领域服务层
├── symmetry/           # 对称性命令 DTO（full-result 计算经 sealed kernel bridge）
├── watcher/            # 文件系统增量监听模块
│   ├── mod.rs          # WatcherState 生命周期管理（start/stop）
│   ├── filter.rs       # 路径过滤规则（隐藏文件/扩展名白名单/忽略文件夹）
│   └── handler.rs      # 事件回调（分类/去重/IPC 发送）
├── ai/                 # AI 模块（按职责拆分）
│   ├── mod.rs          # AiConfig 定义与统一 re-export
│   ├── embedding.rs    # Embedding 请求、LRU 缓存与并发控制
│   ├── chat.rs         # 流式 RAG 对话与 Ponder 节点生成
│   ├── similarity.rs   # 余弦相似度计算
│   └── vector_cache.rs # 向量内存缓存与 top-k BinaryHeap 查询
├── error.rs            # 类型化错误处理（AppError / AppResult）
├── models.rs           # 数据模型
└── lib.rs              # 应用入口
```

## 架构演进（近期）

- **PDF 渲染引擎迁移（v1.0.5）**：PDFium → pdf.js，渲染完全在前端 Canvas 完成（零 IPC），秒开体验；移除 pdfium-render / webp / base64 三个 crate，二进制更小编译更快；新增手绘涂写批注（Pointer Events + 压感），笔迹平滑算法（Douglas-Peucker 简化 + Catmull-Rom 插值）由 Tauri `spawn_blocking` 调用 sealed C++ bridge 并在 C++ kernel 执行；PDF Viewer 模块化拆分为纯渲染壳层 + 4 个子 hook + 7 个 CSS 子文件
- **全量性能优化（v1.0.5）**：15 项优化，补齐 `VectorCacheState` top-k 堆顺序与缓存生命周期同步，纳入晶格引擎。包括保存队列 Map 化、增量监听链路替代全库扫描、向量检索内存缓存 + top-k BinaryHeap、面板拖拽 CSS var 零渲染、图谱/文件树 FNV-1a 指纹修正；晶格计算后续已收口为 C++ kernel full-result ABI
- **计算层 Rust 迁移（v1.0.4）**：6 项前端重计算（语义提取、标签树、图谱索引、热力图、化学计量、数据库归一化）下沉到 Rust 后端，新增 7 个 Tauri 命令
- **架构级优化（v1.0.4）**：修复双重 scan_vault、事件驱动替代轮询、全局笔记缓存、乐观 UI、批量 SQL、复合索引、FTS 延迟填充
- **渲染层优化（v1.0.4）**：CSS hover 替代 DOM 操作、关键组件 memo、文件树 memoize
- **组件拆分与 Hook 提取（v1.0.4）**：6 个 300-510 行的”上帝组件”按单一职责拆分，新增 11 个文件（6 个 hooks + 5 个子组件），每个文件只做一件事
- **前端 App 容器瘦身**：`App.tsx` 已从”状态 + 业务 + 渲染”混合体拆分为编排层，核心逻辑下沉到 hooks 与 app-level 组件
- **前端职责拆分**：`components/app/` 继续细化为工作区壳层、运行时、编辑视口与 `ActiveNoteContent`，降低渲染分发复杂度
- **会话逻辑分层**：`useVaultSession` 现在负责编排，扫描、活动内容、二进制预览、保存队列分别下沉到独立 hooks
- **保存链路增强**：新增保存指纹去重、排队写盘与显式 flush，减少频繁写盘和切换文件时的状态风险
- **语义与 AI 性能优化**：语义共鸣改为上下文提取 + 缓存 + 自适应防抖，AI 侧边栏改为历史消息与流式消息分离渲染
- **测试基建补齐**：引入 `Vitest + jsdom`，为类型、设置和语义推荐逻辑提供基础单元测试
- **跨界面一致性**：主题变量体系覆盖浅色/深色，TRUTH_SYSTEM、设置页、侧边栏等模块统一适配
- **Rust 命令模块化**：`commands.rs` 从集中式文件拆分到 `commands/` 子模块，便于按领域维护与测试
- **AI 模块拆分**：`ai.rs` 按职责拆分为 `ai/` 子模块（embedding / chat / similarity），提升可维护性
- **共享与服务层**：`shared/` 与 `services/` 承担公共能力与领域逻辑，降低命令层重复代码
- **多语言系统**：`i18n/` 基于 React Context 的轻量翻译方案，`useT()` hook 驱动全组件树翻译切换，支持参数化插值
- **类型化错误处理**：Rust 侧引入 `thiserror` 定义 `AppError` 枚举，替换全部 `Result<T, String>`，错误类型可序列化传递给前端
- **React Compiler**：集成编译器自动优化，消除手动 `useMemo`/`useCallback` 的维护负担
- **全局错误边界**：`ErrorBoundary` 组件包裹所有 Suspense 区域，确保单一模块崩溃不影响全局
- **测试基建扩充**：测试用例从 13 增至 38，覆盖核心 hooks（持久化、防抖、文件操作）和 ErrorBoundary
- **化学绘图板**：移除 @xyflow/react 通用画布，引入 Ketcher 专业分子编辑器（.mol 格式），CSS 穿透实现绝对极简暗色主题，/chemdraw 斜杠命令支持 Markdown 内联分子插入

## License

[MIT](LICENSE)
