<p align="center">
  简体中文 | <a href="README_EN.md">English</a>
</p>

<p align="center">
  <img src="apps/tauri/.logo/Logo.png" width="116" alt="Nexus Logo" />
</p>

<h1 align="center">Nexus Kernel · Chemistry_Obsidian</h1>

<p align="center">
  <strong>从 Rust 后端迁移到 C++ sealed kernel 的 Nexus 内核重构版。</strong>
</p>

<p align="center">
  面向本地优先、化学科研/学习场景的智能知识管理桌面应用。
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-24C8DB?logo=tauri&logoColor=white" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61DAFB?logo=react&logoColor=111111" alt="React 19" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20" />
  <img src="https://img.shields.io/badge/Rust-host_bridge-B7410E?logo=rust&logoColor=white" alt="Rust host bridge" />
  <img src="https://img.shields.io/badge/SQLite-local_truth-044A64?logo=sqlite&logoColor=white" alt="SQLite" />
</p>

---

## 项目定位

**Chemistry_Obsidian** 是 [GTC2080/Nexus](https://github.com/GTC2080/Nexus) 的 C++ 内核迁移版本。

原始 Nexus 是一个基于 **Tauri 2 + React 19 + Rust** 的本地优先 AI 知识管理应用，提供类 Obsidian 的 Markdown 知识库、双向链接、标签、LaTeX 数学公式、语义搜索、全局知识图谱、PDF 阅读、AI 问答以及化学学习/科研工具。

这个仓库保留 Nexus 的产品方向，但重构了底层架构的责任边界：

> 把原本沉积在 Rust 后端里的长期业务规则、数据真相和计算逻辑，迁移到一个稳定的 C++20 sealed kernel 中。

Tauri 与 Rust 仍然存在，但它们的角色被收窄为桌面宿主、命令注册、平台适配、网络请求、外部进程启动和 kernel bridge。真正需要长期稳定、可测试、可跨宿主复用的规则，统一由 C++ kernel 持有。

## 为什么要迁移

原来的 Rust 后端可以工作，但随着功能增加，越来越多规则分散在 host 层：

- 路径归一化、默认 limit、query shape、DTO 映射规则容易重复出现。
- AI、图谱、学习统计、PDF、化学计算和 product compute 行为可能在不同模块里漂移。
- 桌面 glue code 和长期业务逻辑混在一起，维护成本越来越高。
- 如果未来出现新的 host，需要重新实现大量规则，而不是复用一套稳定 kernel contract。

这次迁移的目标不是“换语言”，而是把项目从应用后端推进成平台内核：

| 旧结构 | 新结构 |
| --- | --- |
| Rust 后端承担大部分业务规则 | C++ sealed kernel 持有长期真相 |
| Tauri command 同时做编排和规则判断 | Tauri Rust 退回薄桥接层 |
| query / compute 行为容易重复实现 | kernel ABI 定义稳定行为 |
| host 逻辑难以复用 | kernel surface 与 host 解耦 |
| 存储、恢复、校验分散 | kernel 统一持有 SQLite、recovery、validation gate |

## 产品能力

Nexus Kernel 仍然是一个个人知识工作台，但它最强的场景是化学科研与学习：

- **Markdown 知识库**：笔记、双向链接、标签、反链、文件树、标签树、全局知识图谱。
- **AI 知识流**：语义搜索、相关笔记、RAG 上下文构建、Chat / Embedding 管线。
- **PDF 阅读与批注**：pdf.js 渲染、高亮、手写 ink、metadata、anchor、批注持久化。
- **化学工作台**：Ketcher 分子编辑、分子预览、波谱解析、化学计量、高分子动力学、晶体工具、点群/对称性分析、逆合成接口。
- **学习与反馈系统**：study session、heatmap、学习统计、TRUTH_SYSTEM / product compute 反馈。
- **本地优先存储**：vault 文件与 kernel SQLite 状态默认留在本机。

## 架构概览

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

## 仓库结构

```text
.
|-- README.md                 # 中文 README
|-- README_EN.md              # English README
|-- apps/
|   `-- tauri/                # 当前桌面 host：React + Tauri + Rust bridge
|-- docs/                     # 仓库级集成与结构说明
`-- kernel/                   # C++20 sealed kernel、测试、文档、benchmark
    |-- include/kernel/       # public C ABI
    |-- src/internal/         # kernel 内部头文件
    |-- src/impl/core/        # 按 public surface 分组的核心实现
    |-- tests/                # API / parser / search / watcher 测试
    |-- benchmarks/           # startup / IO / rebuild / query gate
    `-- docs/                 # ADR、contract、regression matrix、governance
```

## Kernel Core 布局

`kernel/src/impl/core/` 已经按 public surface 分组：

```text
kernel/src/impl/core/
|-- ai/
|-- attachments/
|-- chemistry/
|-- crystal/
|-- diagnostics/
|-- domain/
|-- notes/
|-- pdf/
|-- product/
|-- runtime/
|-- search/
|-- study/
`-- symmetry/
```

不直接暴露 C ABI 的领域引擎继续放在 core 旁边，例如 `storage/`、`search/`、`pdf/`、`watcher/`、`chemistry/`、`crystal/` 和 `symmetry/`。

## Host / Kernel 边界

Tauri host 通过以下文件调用 kernel：

- `apps/tauri/src-tauri/native/sealed_kernel_bridge.h`
- `apps/tauri/src-tauri/native/sealed_kernel_bridge.cpp`
- `apps/tauri/src-tauri/src/sealed_kernel.rs`

判断规则很简单：

> 只要某条规则影响 durable state、query shape、compute output、recovery、跨 host 兼容性或长期产品语义，就应该进入 kernel。

host 不应该重复持有这些 kernel-owned 规则：

- vault path normalization
- file-extension derivation
- note catalog default limit
- graph / tag / backlink / search construction
- AI embedding cache shape
- RAG context formatting
- PDF metadata / annotation state
- chemistry / crystal / symmetry / product compute behavior
- study session storage and aggregation

## 当前状态

- **来源项目**：[GTC2080/Nexus](https://github.com/GTC2080/Nexus)
- **当前 host**：Tauri desktop app
- **迁移目标**：Rust backend rules -> C++ sealed kernel
- **Kernel milestone**：`stage-phase2-track5-gated`
- **Phase 1**：host-stable kernel baseline complete
- **Phase 2 Track 1-5**：complete and gated
- **当前开发姿态**：继续缩小 host-side 重复规则，把 durable behavior 收口到 kernel contracts 后面。

## 环境要求

- Windows
- Visual Studio 2022 Build Tools with C++ workload
- CMake 3.21+
- Node.js 18+
- Rust 1.77+ 以及 Tauri 2 prerequisites

在这台机器上，kernel 构建环境通常从这里进入：

```powershell
E:\Dev\bin\kernel-dev-x64.cmd
```

## Kernel 命令

kernel 命令从 `kernel/` 目录运行：

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

kernel 构建输出位于 `kernel/out/build`。

## App 命令

app 命令从 `apps/tauri/` 目录运行：

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

## 开发原则

- `README.md` 是中文入口，`README_EN.md` 是英文入口。
- 行为规则写入 `kernel/docs/contracts/`。
- 回归预期写入 `kernel/docs/regression/`。
- 长期架构决策写入 `kernel/docs/adr/`。
- 生成物、本地 IDE 状态、构建目录不要进 Git。
- 不要在 host 层新增 kernel 已经应该持有的规则副本。

## 项目血统

Nexus Kernel 不是一个新的产品想法，而是 Nexus 的架构硬化路线：

```text
Nexus
  local-first AI knowledge workspace
      |
      v
Nexus Kernel / Chemistry_Obsidian
  保留产品方向，把长期真相迁移到 C++ sealed kernel
```

目标很明确：让用户面对的知识工作台保持灵活、好用、适合科研学习；同时让底层 truth model 更难漂移、更容易测试，并具备跨 host 复用的基础。
