<p align="center">
  简体中文 | <a href="README_EN.md">English</a>
</p>

<p align="center">
  <img src="apps/tauri/.logo/Logo.png" width="116" alt="Nexus · Scientist Obsidian Logo" />
</p>

<h1 align="center">Nexus · Scientist Obsidian</h1>

<p align="center">
  <strong>面向科学学习与研究的本地优先智能知识工作台。</strong>
</p>

<p align="center">
  把 Markdown 笔记、论文阅读、化学工具、知识图谱、AI 问答和学习反馈放进同一个桌面应用。
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-24C8DB?logo=tauri&logoColor=white" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61DAFB?logo=react&logoColor=111111" alt="React 19" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20" />
  <img src="https://img.shields.io/badge/SQLite-local_truth-044A64?logo=sqlite&logoColor=white" alt="SQLite" />
</p>

---

## 这个项目是什么

**Nexus · Scientist Obsidian** 是一个桌面端科学知识管理应用，目标是给学习者、研究者和重度笔记用户提供一个更完整的本地工作台。

它不像传统笔记软件只负责保存文本，也不像单一 AI 工具只负责问答。它把知识库、论文阅读、化学计算、语义搜索、图谱导航和学习反馈合在一起，让你的资料、想法、结构化工具和 AI 上下文留在同一个工作流里。

项目默认采用本地优先设计：你的 vault 文件、索引状态和核心数据都优先保存在本机。AI 能力可以接入外部模型，但应用本身不把云端作为知识库的默认归宿。

## 适合谁

- 需要长期维护 Markdown 知识库的学生、研究者和工程用户。
- 经常阅读 PDF、整理论文、建立知识链接的人。
- 需要化学绘图、分子预览、谱图解析、化学计量、晶体/对称性工具的学习与科研场景。
- 想把 AI 问答、语义搜索和 RAG 上下文接入个人资料库，而不是停留在一次性聊天的人。
- 喜欢 Obsidian 式本地知识库，但希望科学工具和 AI 工作流更深地集成在桌面应用里的人。

## 核心能力

### 知识库

- Markdown 笔记、文件树、标签树、双向链接和反链。
- LaTeX 数学公式、任务列表、表格、代码块和富文本编辑体验。
- 全局知识图谱，用节点关系查看笔记、标签和链接结构。

### 论文与资料阅读

- PDF 阅读、高亮、手写 ink、批注、metadata 和 anchor 持久化。
- 笔记与文献可以在同一个 vault 中组织，减少资料和想法分散。

### 科学与化学工具

- Ketcher 分子编辑与分子预览。
- 波谱解析、化学计量、高分子动力学、晶体工具。
- 点群/对称性分析和逆合成相关接口。

### AI 知识流

- 语义搜索、相关笔记、embedding cache 和 RAG 上下文构建。
- Chat 工作流可以围绕当前 vault、当前笔记和检索结果展开。
- AI 只是知识工作流的一部分，而不是替代你的本地资料库。

### 学习反馈

- study session、学习统计和 heatmap。
- product compute / TRUTH_SYSTEM 反馈面板，用于观察知识库和学习行为的状态。

## 架构概览

Nexus · Scientist Obsidian 使用 React + Tauri 构建桌面体验，并把长期稳定的规则和数据真相收口到 C++20 sealed kernel 中。

```text
React 19 UI
    |
    | Tauri commands
    v
Rust host bridge
    |
    | sealed_kernel_bridge.*
    v
C++20 sealed kernel
    |
    +-- SQLite storage
    +-- vault / path rules
    +-- note / search / tag / graph query surfaces
    +-- PDF / chemistry / symmetry / crystal / AI / study / product compute contracts
    +-- recovery and regression gates
```

这个分层的目的很直接：桌面 host 专注窗口、平台能力和交互编排；kernel 持有会影响长期数据、查询行为、计算结果和跨宿主兼容性的规则。

## 仓库结构

```text
.
|-- README.md                 # 中文 README
|-- README_EN.md              # English README
|-- apps/
|   `-- tauri/                # 当前桌面应用：React + Tauri + Rust bridge
|-- docs/                     # 仓库级集成与结构说明
`-- kernel/                   # C++20 sealed kernel、测试、文档、benchmark
    |-- include/kernel/       # public C ABI
    |-- src/internal/         # kernel 内部头文件
    |-- src/impl/core/        # 按 public surface 分组的核心实现
    |-- tests/                # API / parser / search / watcher 测试
    |-- benchmarks/           # startup / IO / rebuild / query gate
    `-- docs/                 # ADR、contract、regression matrix、governance
```

`kernel/src/impl/core/` 已按 public surface 分组，包括 AI、attachments、chemistry、crystal、diagnostics、domain、notes、PDF、product、runtime、search、study 和 symmetry 等模块。

## 当前状态

- **项目名称**：Nexus · Scientist Obsidian
- **目标仓库名**：`Nexus-Scientist-Obsidian`
- **当前 host**：Tauri desktop app
- **核心内核**：C++20 sealed kernel
- **Kernel milestone**：`stage-phase2-track5-gated`
- **Phase 1**：host-stable kernel baseline complete
- **Phase 2 Track 1-5**：complete and gated

## 快速开始

### 环境要求

- Windows
- Visual Studio 2022 Build Tools with C++ workload
- CMake 3.21+
- Node.js 18+
- Rust 1.77+ 以及 Tauri 2 prerequisites

在这台机器上，kernel 构建环境通常从这里进入：

```powershell
E:\Dev\bin\kernel-dev-x64.cmd
```

### Kernel

```powershell
cd kernel
cmake --preset dev
cmake --build --preset build-debug
ctest --preset test-debug
```

常用 focused targets：

```powershell
cmake --build --preset build-debug --target chem_kernel
cmake --build --preset build-debug --target kernel_api_tests
cmake --build --preset build-debug --target kernel_phase_gate
cmake --build --preset build-release
```

### Desktop App

```powershell
cd apps/tauri
npm install
npm run dev
npm run build
npm run test:run
npx tauri dev
npx tauri build
```

`npm run dev` 启动 Vite 前端；`npx tauri dev` 启动完整桌面应用路径。

## 文档地图

这个仓库只保留中文和英文两份 README。细节文档放在更准确的位置：

- 仓库结构：`docs/repo-structure.md`
- 集成计划：`docs/integration-plan.md`
- kernel 当前状态：`kernel/docs/status/kernel-phase1-status.md`
- kernel 架构规则：`kernel/docs/architecture/multi-discipline-kernel-design-rules-v1.md`
- kernel ADR：`kernel/docs/adr/`
- kernel contracts：`kernel/docs/contracts/`
- kernel public surfaces：`kernel/docs/surfaces/`
- kernel regression matrices：`kernel/docs/regression/`
- kernel governance：`kernel/docs/governance/`

## 项目血统

Nexus · Scientist Obsidian 延续自 [GTC2080/Nexus](https://github.com/GTC2080/Nexus) 的产品方向：本地优先、面向知识管理、AI 辅助和科学学习。

原先的 Rust 后端 Nexus 将不再作为主要更新线继续推进。后续功能开发、架构演进和 kernel 工作重心转移到本仓库。

```text
Nexus
  local-first AI knowledge workspace
      |
      v
Nexus · Scientist Obsidian
  science-focused local knowledge workspace
```
