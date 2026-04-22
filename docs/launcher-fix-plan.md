# Launcher Page Fix Plan

> **Scope:** Renderer layer only. No main/preload/kernel changes.  
> **Goal:** Fix the current Launcher page so it truly behaves like an Obsidian Vault Launcher, not a functional homepage or developer tool.

---

## Part 1: 当前 Launcher 页需要修正的具体点

### 1.1 状态语义冲突：右上角显示 `Unavailable`

**问题：** 当 host 可用、session 未打开时，`_renderLauncher` 仍把 `runtime.getSummary()` 传给 `createRuntimeStatusBadge`。此时 `kernel_runtime.index_state` 为 `"unavailable"`，badge 渲染红色 dot + "Unavailable"。用户看到的是：
- 页面让我打开 Vault
- 但右上角说 Unavailable

这两个信号互相矛盾。

**怎么改：**
- Launcher 模式的 top bar **不再渲染 `runtime-status-badge`**
- 改为一个极简中性文本：`"No vault open"`，灰色，不显眼
- 或者干脆只保留应用名，右侧什么都不放
- `runtime-status-badge` 只保留给 Workspace 模式的 top bar 使用

### 1.2 Recent Vaults 不像真正的列表项

**问题：** 当前 `recent-vaults-list.js` 把完整路径作为唯一文本显示，没有 vault 名称的概念，长路径会溢出，hover 态太弱。

**怎么改：**
- 从路径提取 vault 名称（路径最后一段文件夹名）
- 主标题：vault 名称（14px, 深色）
- 次级：完整路径（12px, 灰色，ellipsis 截断）
- hover 时背景变深、左边加 2px 指示条
- active/pressed 时背景再深一级

### 1.3 Open Vault 按钮交互逻辑不自然

**问题：** 当前 Open Vault 按钮第一次点击展开输入框，第二次点击才提交。用户不知道要按两次。这像一个彩蛋，不像主流程。

**怎么改：**
- Open Vault 按钮 = "展开输入区"，明确告诉用户"我要输入路径"
- 输入区内有自己的提交按钮（"Open"）
- 或者：Open Vault 直接展开输入区并 focus，用户填完按 Enter 或点 Open 提交
- 整体语义从"一个按钮两种行为"变成"主按钮触发次级输入流程"

### 1.4 Launcher 卡片内部信息层次不够清晰

**问题：** Recent Vaults、Divider、Open Vault、Create Vault 平铺在同一层级，用户一眼看不出主次。

**怎么改：**
- **第一区块：Recent Vaults**
  - 标题（小写大写字母，灰色）
  - 列表或空态
- **第二区块：Actions**
  - `Open Vault` 主按钮（深色背景，全宽）
  - `Create New Vault` 次级按钮（边框样式，全宽）
  - 路径输入区（点击 Open Vault 后展开，属于 secondary flow）
- **第三区块：Error**
  - 固定底部，只在失败时出现

---

## Part 2: 状态语义修正方案

### 2.1 三层状态优先级（从高到低）

| 优先级 | 状态 | UI 表现 | 说明 |
|---|---|---|---|
| P0 | **Host unavailable** | 全屏阻断 unavailable surface，不渲染 launcher 任何元素 | bootstrap 失败、preload 缺失 |
| P1 | **Opening vault** | Launcher card 上叠加半透明 loading 遮罩，所有操作禁用 | 过渡态 |
| P2 | **No vault open** | 显示完整 Vault Launcher | 正常入口态 |

### 2.2 关键规则

- **P0 出现时，P1/P2 绝不出现。** 如果 host unavailable，用户看不到 launcher 的任何元素。
- **P1 出现时，P2 的交互被冻结。** Launcher 仍然可见，但被遮罩覆盖，按钮禁用。
- **Launcher 模式下，不显示 runtime 诊断信息。** 顶部只保留应用名 + 可选的 "No vault open" 中性文本。
- `createRuntimeStatusBadge` 只在 Workspace 模式使用。

### 2.3 具体代码修改点

在 `app-shell.js` 的 `_renderLauncher` 中：
- 移除 `createRuntimeStatusBadge(runtimeEnv)` 的传入
- `launcher-shell.js` 的 top bar 右侧改为固定文本 `"No vault open"` 或留空
- 只有在 `_renderUnavailable` 时才使用 `createStateSurface("unavailable")`

---

## Part 3: Recent Vaults 列表项改造方案

### 3.1 Vault Item 结构

每个 recent vault item 包含：

```
┌─────────────────────────────────────┐
│ 📁  MyVault                         │  ← 主标题：vault 名称（14px, #111827）
│     C:\Users\...\MyVault            │  ← 次级：路径（12px, #6b7280, ellipsis）
└─────────────────────────────────────┘
```

### 3.2 路径到名称的提取规则

```javascript
function extractVaultName(path) {
  if (!path || typeof path !== "string") return "Unknown";
  // Windows: use backslash, Unix: use forward slash
  const sep = path.includes("\\") ? "\\" : "/";
  const parts = path.split(sep).filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : path;
}
```

### 3.3 视觉规则

- **默认态：** 白色背景，1px `#e5e7eb` 边框，圆角 8px
- **Hover 态：** 背景 `#f9fafb`，左边框 2px `#111827`
- **Active/Pressed 态：** 背景 `#f3f4f6`
- **路径文本：** `white-space: nowrap; overflow: hidden; text-overflow: ellipsis;`
- **图标：** 📁 固定前缀，与文本有 10px gap

### 3.4 空态

- 背景 `#f9fafb`，圆角 8px
- 文字：`"No recent vaults. Open or create one to get started."`
- 颜色：`#9ca3af`，居中

### 3.5 点击行为

- 整个 item 可点击
- 点击直接触发 `session.openVault(path)`
- 点击后 item 立即变为禁用态（防止重复点击）

---

## Part 4: Launcher 卡片重排方案

### 4.1 保留的组件

- `launcher-shell.js` — 外壳保留，top bar 右侧逻辑调整
- `recent-vaults-list.js` — 保留，内部结构升级
- `launcher-page.js` — 保留，内部层次重排
- `addRecentVault` / `readRecentVaults` — 完全保留

### 4.2 替换/重排的内容

**Top Bar（launcher-shell.js）：**
- 左侧：`Chemistry Obsidian`（不变）
- 右侧：移除 `statusBadge` 参数，改为固定文本 `"No vault open"`（12px, `#9ca3af`）或完全留空

**Card 内部（launcher-page.js）：**

```
┌─────────────────────────────────────┐
│ Recent Vaults                       │  ← 小标题
│ ┌─────────────────────────────────┐ │
│ │ 📁 MyVault                      │ │
│ │    C:\Users\...\MyVault        │ │
│ └─────────────────────────────────┘ │
│ ┌─────────────────────────────────┐ │
│ │ 📁 AnotherVault                 │ │
│ │    C:\Users\...\AnotherVault   │ │
│ └─────────────────────────────────┘ │
├─────────────────────────────────────┤
│ [      Open Vault        ]          │  ← 主按钮，深色
│ [    Create New Vault    ]          │  ← 次级按钮，边框
│                                     │
│ ┌─────────────────────────────────┐ │  ← 展开后显示
│ │ /path/to/vault          [Open]  │ │
│ └─────────────────────────────────┘ │
├─────────────────────────────────────┤
│ [Error block, only on failure]      │
└─────────────────────────────────────┘
```

**Open Vault 交互流程：**
1. 用户点击 "Open Vault" 按钮
2. 按钮下方展开路径输入区（带淡入动画，max-height 过渡）
3. 输入区自动 focus
4. 用户输入路径，按 Enter 或点 "Open" 提交
5. 提交后触发 `onOpenVault(path)`，整个 card 进入 loading 遮罩态

**错误展示：**
- 固定在 card 底部
- 只在 `lastError` 存在时出现
- 不占用常驻空间

### 4.3 明确不引入的新东西

- 不加入图标库（继续用 emoji 或纯文字）
- 不加入复杂动画（只保留 max-height 过渡）
- 不加入路径自动补全
- 不加入文件夹选择对话框（仍是 host gap）
- 不加入 vault 删除/重命名管理

---

## Part 5: 分批 backlog

---

### Batch 1: 状态语义修正 + Top Bar 清理

#### 目标
消除 Launcher 模式下右上角 `Unavailable` 的误导性状态，把状态语义彻底拧顺。

#### 为什么现在做
这是当前用户最直观的困惑来源。如果不先修这个，后面 visual 改得再好，用户仍会觉得"系统坏了"。

#### 要改哪些 renderer 文件

1. **`components/layout/launcher-shell.js`**
   - 移除 `statusBadge` 参数
   - top bar 右侧改为固定文本 `"No vault open"`（灰色，12px）或完全留空

2. **`app-shell.js`**
   - `_renderLauncher` 中不再创建 `createRuntimeStatusBadge(runtimeEnv)`
   - 确认 `_renderUnavailable` 和 `_renderLauncher` 互斥逻辑正确

#### 明确不做什么
- 不改 `runtime-status-badge.js` 本身（它在 Workspace 模式仍正常工作）
- 不改 Workspace 模式的 top bar
- 不引入新的状态机

#### 依赖哪些现有 `window.hostShell.*`
- `bootstrap.getInfo()` — 判断 host 可用性（已有）

#### 是否存在 Explicit Host Gap
- **无**

#### 验收标准
- [ ] 未打开 vault 且 host 可用时，launcher top bar 右侧不显示红色 `Unavailable`
- [ ] 未打开 vault 且 host 可用时，launcher 正常显示，不触发 unavailable surface
- [ ] Host unavailable 时，全屏阻断，launcher 完全不渲染
- [ ] Smoke 测试仍然通过

---

### Batch 2: Recent Vaults 列表项升级

#### 目标
把 Recent Vaults 从"路径文本框"改成真正的"vault 列表项"。

#### 为什么现在做
Recent Vaults 是 Launcher 的核心内容区。如果它不像列表项，整个 Launcher 的"选择空间"语义就立不住。

#### 要改哪些 renderer 文件

1. **`pages/launcher/recent-vaults-list.js`**
   - 新增 `extractVaultName(path)` 函数
   - 每个 item 渲染为两行结构：主标题（vault 名）+ 次级路径
   - 路径加 `text-overflow: ellipsis`
   - 增强 hover/active 态（背景色 + 左边框指示器）
   - 空态保持现有逻辑，微调样式

2. **`pages/launcher/launcher-page.js`**（微小调整）
   - 确认 `recentList` 的 margin/padding 与新 item 样式协调

#### 明确不做什么
- 不做路径验证
- 不做 vault 图标自定义
- 不做拖拽排序
- 不引入外部图标库

#### 依赖哪些现有 `window.hostShell.*`
- 无新增依赖（localStorage 已有）

#### 是否存在 Explicit Host Gap
- **无**

#### 验收标准
- [ ] Recent vault item 显示 vault 名称（主标题）+ 路径（次级）
- [ ] 长路径自动 ellipsis 截断，不撑破布局
- [ ] Hover 时有明确视觉反馈（背景变深 + 左边框）
- [ ] 点击整个 item 触发打开
- [ ] 空列表时显示正确的空态文字

---

### Batch 3: Launcher 卡片内部重排 + Open Vault 交互修正

#### 目标
把 Launcher 卡片内部的信息层次重排，让 Open Vault 的输入流程成为明确的 secondary flow。

#### 为什么现在做
这是把 Launcher 从"表单盒子"变成"启动器"的最后一步。信息层次对了，用户的认知负担才会降下去。

#### 要改哪些 renderer 文件

1. **`pages/launcher/launcher-page.js`**（主要修改）
   - 重排内部结构：Recent Vaults → Actions → Error
   - Open Vault 按钮点击后直接展开输入区（而不是两步提交）
   - 输入区包含路径 input + Open 提交按钮
   - 输入区展开时有 max-height 过渡动画
   - Create New Vault 保持占位
   - Error 区域固定在底部，不占常驻空间
   - Loading 遮罩覆盖整个 card（保持现有逻辑）

2. **`components/layout/launcher-shell.js`**（微小调整）
   - 确认 card padding 与新内部结构协调

#### 明确不做什么
- 不引入复杂动画库
- 不修改路径输入的底层逻辑（仍走 `session.openVault`）
- 不做 Create Vault 的真实功能
- 不改 Workspace 任何代码

#### 依赖哪些现有 `window.hostShell.*`
- `session.openVault()` — 打开 vault（已有）

#### 是否存在 Explicit Host Gap
- **无**

#### 验收标准
- [ ] Launcher 卡片内部层次清晰：Recent Vaults / Actions / Error
- [ ] Open Vault 按钮点击后展开输入区，有明确过渡
- [ ] 输入区有独立提交按钮，Enter 键可提交
- [ ] 输入区未展开时，页面不以输入框为主角
- [ ] Opening vault 时 card 上显示 loading 遮罩
- [ ] Smoke 测试仍然通过

---

## 横向约束

1. **只能改 Renderer**，不碰 main/preload/kernel
2. **不改 host API contract**
3. **不新增 Host Gap**
4. **保持 smoke 兼容**：`index.html` 的 smoke 标记保留
5. **不改 Workspace 任何代码**：这一轮只修 Launcher
