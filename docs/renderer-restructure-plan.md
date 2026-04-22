# Renderer Restructure Plan — Vault Launcher + Workspace Shell

> **Scope:** Renderer layer only. No main/preload/kernel changes.  
> **Goal:** Shift from "functional homepage" to "Vault Launcher → Workspace Shell" paradigm, matching Obsidian-like interaction model.

---

## Part 1: 当前 Renderer 的结构问题

### 1.1 范式错位：Launch 页不像 Launcher

当前 `welcome-page.js` 是一个**开发者工具面板**：中央一个路径输入框 + "Open Vault" 按钮。这不是用户启动知识库时的心理模型。用户期望看到的是：

- 我有哪些 Vault？
- 我要打开哪一个？
- 新建一个 Vault 的入口在哪？

当前页面没有"选择空间"的语义，只有"填写表单"的语义。

### 1.2 导航层级说谎

当前 `nav-bar.js` 在 **vault 未打开时** 就显示 Search / Attachments / Chemistry / Diagnostics。这些按钮被 disabled，但视觉上它们是"一级导航"。这向用户传递了错误信息："这些工具在 vault 外也存在，只是暂时点不了"。事实是：**这些工具在 vault 外根本不存在**。它们在信息架构上是 vault 打开后的子功能，不是应用级入口。

### 1.3 状态语义冲突

当前 `welcome-page` 中央显示 "Open a Vault"，右上角同时有一个 `runtime-status-badge`。当 host unavailable 时：

- badge 变红
- 但 "Open a Vault" 表单仍然可见
- 用户困惑："到底能不能打开？"

**Host unavailable** 和 **No vault open** 是两种完全不同的状态，当前 UI 把它们混在了一起。

### 1.4 工作区结构缺失

打开 vault 后，当前布局是：

```
Top Bar (tabs) → Nav Bar (Search/Attach/Chem/Diag) → Content
```

这是**管理后台（Admin Dashboard）**范式，不是**知识库工作区**范式。Obsidian 的范式是：

```
Top Bar (light) → Sidebar (files/tools) → Main Content
```

当前缺少：
- 左侧文件/功能导航
- 默认内容视图（打开后不应该让用户再选一次"我要干嘛"）
- 内容中心（Content-centric）而非功能中心（Feature-centric）

### 1.5 内容中心缺位

当前 `vault-page.js` 是一个功能选择占位页："Vault is open. Select a section from the navigation." 打开 vault 后，用户看到的是一片空白加一句提示。这要求用户再做一次决策。正确的工作区范式是：打开后立即呈现内容（文件树、最近文件、或默认笔记），工具围绕内容存在，而不是内容围绕工具存在。

---

## Part 2: Vault Launcher 方案

Vault Launcher 是用户进入系统前的**唯一主界面**。它应该被理解为"选择你要进入的空间"，而不是"配置一个会话"。

### 2.1 结构

```
┌─────────────────────────────────────────┐
│  App Name          [light status]        │  ← 顶部：很轻，不抢戏
├─────────────────────────────────────────┤
│                                         │
│         ┌─────────────────────┐         │
│         │   Recent Vaults     │         │
│         │   - vault A         │         │
│         │   - vault B         │         │
│         │   [Empty state]     │         │
│         ├─────────────────────┤         │
│         │   [  Open Vault  ]  │         │
│         │   [ Create Vault ]  │         │
│         └─────────────────────┘         │
│                                         │
└─────────────────────────────────────────┘
```

### 2.2 必须出现的内容

- **Recent Vaults 列表**
  - 每个条目显示 vault 路径（或名称）
  - 点击直接打开
  - 空列表时显示空态："No recent vaults. Open or create one to get started."
  - 数据来源：renderer 本地 `localStorage`（无需 host API）
- **Open Vault 主按钮**
  - 点击后展开/激活路径输入区域
  - 路径输入保留，但不再是页面的唯一主角
  - 提交后进入 opening 状态
- **Create Vault 次级入口**
  - 可以暂时是占位按钮（显示 "Coming soon" 或 toast）
  - 但不能没有这个位置
- **固定错误区**
  - 在 card 底部预留错误展示位
  - open vault 失败时稳定显示，不跳出不相关界面

### 2.3 绝不能出现的内容

- Search / Attachments / Chemistry / Diagnostics 的任何导航入口
- 详细 runtime 数据面板
- session-status-card 这种诊断式组件
- 顶部 tab bar 或功能页签

### 2.4 状态表现

| 状态 | Launcher 表现 |
|---|---|
| Host available, no vault | 显示完整 launcher card |
| Opening vault | Launcher card 上叠加半透明 loading 遮罩，按钮禁用，显示 "Opening vault..." |
| Open failed | 错误信息显示在 card 底部固定区域，launcher 仍然可见 |
| Host unavailable | **全屏阻断层**，不显示 launcher 任何元素。只有 "Host unavailable" + Retry |

---

## Part 3: Workspace Shell 方案

一旦 vault 打开，整个 Renderer 必须切换到 **Workspace Shell** 模式。这不是"在 launcher 上加个导航"，而是**布局范式的彻底切换**。

### 3.1 结构

```
┌─────────────────────────────────────────────────────────┐
│  App Name   |   Vault: MyVault   |   [green dot] Ready  │  ← Top Bar：很轻
├─────────────┬───────────────────────────────────────────┤
│             │                                           │
│  Sidebar    │                                           │
│             │         Main Content Area                 │
│  📁 Files   │                                           │
│  🔍 Search  │    (Files tree / Search results /         │
│  📎 Attach  │     Attachment list / Spectrum list /      │
│  ⚗ Chem     │     Diagnostics / Note content)            │
│  ────────   │                                           │
│  🔧 Diag    │                                           │
│             │                                           │
└─────────────┴───────────────────────────────────────────┘
```

### 3.2 Top Bar

- **左侧**：应用名（很轻，不显眼）
- **中央**：当前 Vault 名称（比应用名更突出）
- **右侧**：
  - 轻量状态 dot + 文本（Ready / Catching up / Rebuilding）
  - Close Vault 按钮
- **不出现**：功能页签、详细 runtime JSON、session 诊断面板

### 3.3 Sidebar（左侧导航）

 Sidebar 是 workspace 的**功能入口**，不是全局导航。它只在 vault 打开后出现。

条目（按优先级排序）：

1. **Files**（默认选中）
   - 当前只是占位，显示 "Files view coming soon"
   - 但结构上必须存在，因为这是 Obsidian 范式的心脏
2. **Search**
3. **Attachments**
4. **Chemistry**
5. **Diagnostics**（用分隔线与上面分开，低频）

 Sidebar 条目选中态：高亮背景 + 左边框指示器。

### 3.4 Main Content Area

- **默认视图**：Files 占位（显示提示文字或空态）
- **切换逻辑**：点击 sidebar 条目，替换 content area 内容，不跳转页面
- **各功能视图**：
  - Search → `search-page.js`（现有逻辑，改为在 content area 内渲染）
  - Attachments → `attachment-page.js`（现有逻辑，改为在 content area 内渲染）
  - Chemistry → `chemistry-page.js`（现有逻辑，改为在 content area 内渲染）
  - Diagnostics → `diagnostics-page.js`（现有逻辑，改为在 content area 内渲染）

### 3.5 与当前模式的根本区别

| 维度 | 当前模式 | Workspace Shell |
|---|---|---|
| 默认视图 | "Select a section" 空白页 | Files 占位（内容中心） |
| 导航位置 | 顶部 tab bar | 左侧 sidebar |
| 功能页关系 | 独立页面跳转 | Content area 内视图替换 |
| 顶部信息 | runtime JSON + status dot | Vault name + light status |

---

## Part 4: 状态语义重整

### 4.1 状态分层

Renderer 必须严格区分三层状态：

```
Layer 1: Host availability      (bootstrap bridge)
Layer 2: Session lifecycle      (none / opening / open / closing)
Layer 3: Runtime health         (ready / catching_up / rebuilding / unavailable)
```

**Layer 1 失败 → 阻断一切。**  不显示 launcher，不显示 workspace，只有 unavailable surface。  
**Layer 2 决定布局模式。**  `none` → Launcher；`open` → Workspace。  
**Layer 3 只影响 workspace 内的 banner 和 badge。**  不改变布局模式。

### 4.2 各状态 UI 规则

| 状态 | 层级 | UI 规则 |
|---|---|---|
| **Host unavailable** | Layer 1 | 全屏阻断，居中显示 unavailable surface + retry。不显示 launcher，不显示 workspace。 |
| **No vault open** | Layer 2 | 全屏 Vault Launcher。不显示 sidebar，不显示功能导航。 |
| **Opening vault** | Layer 2 | Launcher 上叠加 loading 遮罩，禁用输入和按钮，显示 "Opening vault..." |
| **Closing vault** | Layer 2 | Workspace 淡出或显示 loading，然后切回 Launcher。 |
| **Ready** | Layer 3 | 绿色 dot + "Ready"。Sidebar 可用。无 banner。 |
| **Catching up** | Layer 3 | 黄色 dot + "Catching up..." + 非阻断 banner："Index catching up. Search results may be incomplete." |
| **Rebuilding** | Layer 3 | 琥珀色 dot + "Rebuilding..." + 非阻断 banner："Index rebuilding. Search results may be incomplete." |
| **Adapter detached** | Layer 3 | 红色 dot + "Adapter detached" + 非阻断 banner："Kernel adapter detached. Some features may be unavailable." |

### 4.3 状态互斥规则

- 同一时间，**Launcher 和 Workspace 只能出现一个**。
- **Unavailable surface 和 Launcher 不能同时出现**。
- **Degraded banner 和 Unavailable surface 不能同时出现**（unavailable 优先级更高）。
- **Loading 遮罩只覆盖局部**（launcher card 或 content area），不覆盖整个屏幕（除非 host unavailable）。

---

## Part 5: 页面与组件重组方案

### 5.1 目录重组

```
apps/electron/src/renderer/
  index.html                          # 不变，保留 smoke 兼容标记
  app-shell.js                        # 重写：launcher vs workspace 路由切换

  state/
    host-store.js                     # 不变，继续复用

  services/
    host-api-client.js                # 不变
    envelope-guard.js                 # 不变

  components/
    layout/
      launcher-shell.js               # 新增：launcher 布局（居中 card）
      workspace-shell.js              # 新增：workspace 布局（sidebar + content + topbar）
      sidebar.js                      # 新增：左侧功能导航
      # nav-bar.js                    # 废弃：当前顶部 tab bar 不再使用
      # app-layout.js                 # 废弃：被 launcher-shell + workspace-shell 替代

    shared/
      state-surface.js                # 不变，继续复用
      host-error-card.js              # 不变，继续复用
      runtime-status-badge.js         # 保留：简化后用于 top bar
      # session-status-card.js        # 废弃：不再作为全局组件，内容并入 diagnostics-page

  pages/
    launcher/
      launcher-page.js                # 新增：替代 welcome-page.js
      recent-vaults-list.js           # 新增：recent vaults 列表 + 空态

    workspace/
      workspace-home-page.js          # 新增：workspace 默认内容占位（Files 视图）

    # 以下页面保留，但改为 workspace content-area 内渲染：
    search-page.js                    # 保留：内部逻辑不变
    search-filter-bar.js              # 保留
    search-result-list.js             # 保留
    search-result-item.js             # 保留
    search-view-model.js              # 保留
    attachment-page.js                # 保留
    attachment-list.js                # 保留
    attachment-detail.js              # 保留
    attachment-view-model.js          # 保留
    pdf-metadata-card.js              # 保留
    note-attachment-refs.js           # 保留
    chemistry-page.js                 # 保留
    spectrum-list.js                  # 保留
    spectrum-detail.js                # 保留
    spectrum-view-model.js            # 保留
    note-chem-refs.js                 # 保留
    diagnostics-page.js               # 保留
    diagnostics-export-card.js        # 保留
    rebuild-control-card.js           # 保留
    runtime-page.js                   # 保留（可选并入 diagnostics）

    # 以下页面废弃：
    # welcome-page.js                 # 被 launcher-page.js 替代
    # vault-page.js                   # 被 workspace-home-page.js + workspace-shell.js 替代
```

### 5.2 复用策略

**完全不变，直接复用：**
- `services/*` — envelope-guard、host-api-client 逻辑不变
- `state/host-store.js` — store 规则不变
- `components/shared/state-surface.js`、`host-error-card.js` — 四种基础状态组件不变
- `pages/search-*.js` — 搜索逻辑不变，只是挂载点从独立页面变为 content area
- `pages/attachment-*.js`、`pdf-metadata-card.js`、`note-attachment-refs.js` — 附件逻辑不变
- `pages/chemistry-*.js`、`spectrum-*.js`、`note-chem-refs.js` — 化学逻辑不变
- `pages/diagnostics-*.js`、`rebuild-control-card.js`、`runtime-page.js` — 诊断逻辑不变

**需要调整：**
- `app-shell.js` — 路由逻辑从 `welcome ↔ vault` 改为 `launcher ↔ workspace`
- `components/layout/*` — 整体替换为 launcher-shell + workspace-shell + sidebar

**废弃：**
- `welcome-page.js` — launcher-page.js 替代
- `vault-page.js` — workspace-home-page.js + workspace-shell.js 替代
- `nav-bar.js` — sidebar.js 替代
- `app-layout.js` — launcher-shell.js + workspace-shell.js 替代
- `session-status-card.js` — 内容并入 diagnostics-page.js

### 5.3 Recent Vaults 数据来源

Recent vaults 列表使用 `localStorage` 存储最近打开的 vault 路径。这是纯 renderer 行为，不依赖 host API。

- `localStorage.setItem('chem_obsidian_recent_vaults', JSON.stringify(paths))`
- 最大保留 10 条
- 打开成功时自动追加到列表顶部
- 不依赖 host gap

---

## Part 6: 分批 backlog

---

### Batch A: Launcher 结构 + 路由切换

#### 目标
建立 Vault Launcher 作为未打开 vault 时的唯一入口页，重写 app-shell.js 的入口路由逻辑。

#### 为什么现在做
Launcher 是用户的第一印象。如果入口页范式不对，后续 workspace 再调整也会感到割裂。先把"大门"做对。

#### 要改哪些页面/组件

1. **新增 `components/layout/launcher-shell.js`**
   - 居中 card 布局
   - 顶部轻量条（应用名 + 极简状态 dot）
   - 中央 card 区域

2. **新增 `pages/launcher/launcher-page.js`**
   - Recent Vaults 区域（空态占位）
   - Open Vault 主按钮（点击展开路径输入）
   - Create Vault 次级按钮（占位）
   - 固定错误展示区

3. **新增 `pages/launcher/recent-vaults-list.js`**
   - 从 `localStorage` 读取最近 vault 路径
   - 渲染为可点击列表
   - 空列表时显示空态
   - 点击直接触发 `session.openVault`

4. **重写 `app-shell.js`**
   - 路由逻辑：`host unavailable` → unavailable surface
   - `host available + session none` → launcher-shell + launcher-page
   - `host available + session open` → workspace-shell（Batch B 实现）
   - 移除 `_buildLayout` 中对旧 `app-layout.js` 的调用

5. **废弃 `welcome-page.js`、`nav-bar.js`**
   - 文件保留但不再被引用（后续批次清理）

#### 明确不做什么
- 不做 Recent Vaults 的复杂管理（删除、重命名、排序）
- 不做 Create Vault 的真实功能（纯占位按钮）
- 不做动画过渡（淡入淡出）
- 不做主题/皮肤

#### 依赖哪些现有 `window.hostShell.*`
- `bootstrap.getInfo()` — 确认 host 可用
- `session.getStatus()` — 确认 session 状态
- `session.openVault()` — 打开 vault

#### 是否存在 Explicit Host Gap
- **无**。Recent Vaults 使用 `localStorage`，不依赖 host。

#### 验收标准
- [ ] 未打开 vault 时显示 Vault Launcher，不显示功能导航
- [ ] Launcher 包含 Recent Vaults、Open Vault、Create Vault 三个区域
- [ ] Host unavailable 时全屏阻断，不显示 launcher
- [ ] Opening vault 时显示 loading 遮罩，禁用操作
- [ ] Open 失败时错误显示在 launcher card 底部
- [ ] Smoke 测试仍然通过

---

### Batch B: Workspace Shell 结构

#### 目标
建立 Workspace Shell 作为 vault 打开后的唯一布局，用 sidebar + content area 替代当前的顶部 tab bar。

#### 为什么现在做
Workspace 是用户 90% 时间停留的界面。Sidebar + Content 的范式是 Obsidian 体验的核心，必须在此批次建立。

#### 要改哪些页面/组件

1. **新增 `components/layout/workspace-shell.js`**
   - 三栏布局：Top Bar（轻量）+ Sidebar（左侧）+ Content Area（中央）
   - Top Bar 只显示：应用名、vault 名、状态 dot + 文本、Close Vault 按钮
   - 无功能 tab，无详细诊断面板

2. **新增 `components/layout/sidebar.js`**
   - 条目：Files（默认选中）、Search、Attachments、Chemistry、Diagnostics
   - Diagnostics 用分隔线与上方条目分开
   - 选中态：高亮背景 + 左边框指示器
   - 点击切换 content area 内容

3. **新增 `pages/workspace/workspace-home-page.js`**
   - Workspace 默认视图（Files 占位）
   - 显示 "Files view coming soon" + 当前 vault 路径
   - 结构上作为 content-centric 的默认态

4. **重写 `app-shell.js`**
   - vault open 后渲染 `workspace-shell`
   - sidebar 点击切换 content area 内的视图
   - 把现有 `search-page.js`、`attachment-page.js`、`chemistry-page.js`、`diagnostics-page.js` 挂载到 content area

5. **废弃 `app-layout.js`、`vault-page.js`**
   - 文件保留但不再被引用

#### 明确不做什么
- 不做真正的 Files 树（只做空占位）
- 不做 sidebar 拖拽调整宽度
- 不做多面板 split view
- 不做 tab 多开（一个功能一个视图）

#### 依赖哪些现有 `window.hostShell.*`
- `runtime.getSummary()` — top bar 状态 dot
- `session.getStatus()` — vault 名
- `session.closeVault()` — Close Vault 按钮

#### 是否存在 Explicit Host Gap
- **无**。所有需要的数据已由现有 API 提供。

#### 验收标准
- [ ] 打开 vault 后切换到 Workspace Shell
- [ ] 左侧 sidebar 显示 Files / Search / Attachments / Chemistry / Diagnostics
- [ ] 默认选中 Files，中央显示 workspace-home-page
- [ ] 点击 sidebar 条目切换 content area 内容
- [ ] Top Bar 显示 vault 名 + 状态 dot + Close Vault
- [ ] 不出现顶部功能 tab bar
- [ ] Smoke 测试仍然通过

---

### Batch C: 状态语义统一 + 清理

#### 目标
统一 Launcher / Workspace 中的状态表现规则，清理废弃文件，确认 smoke helpers 正常工作。

#### 为什么现在做
Batch A 和 B 建立了新结构，但细节状态（degraded banner、loading 遮罩、error 展示位置）可能不一致。此批次统一这些规则，并清理技术债务。

#### 要改哪些页面/组件

1. **统一 degraded banner 规则**
   - `workspace-shell.js` 的 content area 顶部固定位置显示 degraded banner
   - 规则：catching_up / rebuilding / adapter_detached 时显示
   - banner 非阻断，不遮盖内容

2. **统一 opening / closing 过渡态**
   - Launcher 中 opening：card 内 loading 遮罩
   - Workspace 中 closing：workspace 淡出或显示 "Closing..."，然后切回 launcher

3. **调整 `window.__rendererSmoke`**
   - `getPageName()` 继续返回当前页面标识（`launcher` / `files` / `search` / `attachments` / `chemistry` / `diagnostics`）
   - `navigateTo(pageName)` 继续支持导航
   - 确保 smoke 不依赖 DOM 中已废弃的 `host-shell-marker` 等旧标记

4. **清理废弃文件**
   - 删除或归档：`welcome-page.js`、`vault-page.js`、`nav-bar.js`、`app-layout.js`、`session-status-card.js`

5. **确认 index.html smoke 兼容**
   - 保留 `host-shell-marker`、`host-shell-detail`、title 等 smoke 断言需要的元素
   - 这些元素隐藏存在，不影响新布局

#### 明确不做什么
- 不做复杂动画
- 不做文件树真实功能
- 不做新 host API 需求
- 不做 UI polish（圆角、阴影、字体微调）

#### 依赖哪些现有 `window.hostShell.*`
- 无新增依赖。

#### 是否存在 Explicit Host Gap
- **无**。

#### 验收标准
- [ ] Catching up / Rebuilding / Adapter detached 时显示正确的非阻断 banner
- [ ] Opening vault 时 launcher 显示 loading 遮罩，不冲突
- [ ] Closing vault 时平滑切回 launcher
- [ ] `window.__rendererSmoke.getPageName()` / `navigateTo()` 正常工作
- [ ] 废弃文件已清理或归档
- [ ] Smoke 测试仍然通过
- [ ] 不存在 `nav-bar.js` 的残留引用

---

## 横向约束（贯穿所有批次）

1. **Renderer 边界**：只能消费 `window.hostShell.*`，不碰 Node/Electron/SQLite/state 目录。
2. **Host Gap 规则**：任何需要 host 才能实现的能力，单列为 Explicit Host Gap，不在 renderer 层 workaround。
3. **Smoke 兼容**：`index.html` 中的 smoke 标记（title、host-shell-marker、host-shell-detail）必须保留，可以隐藏但不可删除。
4. **状态唯一性**：`host-store.js` 继续作为唯一真相源，不衍生第二套业务状态。
5. **范式红线**：Vault 未打开时，Search/Attachments/Chemistry/Diagnostics 绝不能作为主导航出现。
