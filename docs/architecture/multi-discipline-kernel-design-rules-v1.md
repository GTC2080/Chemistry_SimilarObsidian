<!-- Reason: This file freezes the kernel-side review rules for future domain capability tracks so chemistry, physics, biology, and other discipline layers can land on the host-stable baseline without hard-wiring domain semantics into the core truth model. -->

# 《多学科兼容的内核设计规则清单 v1》

Last updated: `2026-04-22`

## Scope

本文件用于约束未来学科能力线如何建立在当前内核之上。
本文件是评审规则，不是实现设计，也不重开已冻结的 kernel baseline 架构。

本文件默认以下前提已经冻结：

- host-stable kernel baseline 已成立
- formal search capability surface 已成立
- formal attachment capability surface 已成立
- rebuild / recovery / watcher / diagnostics / gate 基线已成立
- files are truth, SQLite is derived state
- PDF substrate 按独立 capability track 推进，不回退为 reader-shell 讨论

## A. 总原则

多学科兼容的内核只兼容跨学科稳定的对象、引用、修订、重建与诊断纪律，不兼容任何单一学科的专有语义、交互壳层或为未来假设预埋的平台化抽象。

## B. 多学科兼容的内核设计规则清单

### 1. Core Truth Model 规则

- **规则 1.1：`core schema` 只允许承载跨学科通用对象。**
  说明：允许进入 core 的只有 `note`、`attachment`、`source reference`、`anchor`、`path identity`、`content revision`、`presence / ref_state`、以及 rebuild / recovery / diagnostics 所需的通用生命周期状态。

- **规则 1.2：通用真相层只表达身份、位置、引用、修订、存在性和生命周期。**
  说明：core 负责回答“它是什么、在哪、引用了什么、当前是否有效”，不负责回答“它在化学/物理/生物上代表什么实体”。

- **规则 1.3：单一学科对象不得成为 core truth object。**
  说明：`molecule`、`reaction`、`spectrum`、`equation`、`lattice`、`gene`、`protein`、`cell`、`assay` 等对象不得直接进入 core schema。

- **规则 1.4：只有在跨学科稳定且已被多个能力线复用时，才允许新增 core object。**
  说明：评审时必须证明该对象不是某一学科的换皮需求，而是所有学科都需要的公共承载面。

### 2. Domain Semantics 隔离规则

- **规则 2.1：化学 / 物理 / 生物等专有语义不得直接进入 core kernel。**
  说明：任何学科词表、实体分类、关系定义、解释规则都不能直接写进 core enum、core schema、core query 语义或 core diagnostics 结论。

- **规则 2.2：学科语义只能生长在独立的 domain capability layer。**
  说明：domain layer 可以建立在 note、attachment、source reference、anchor 和 search 之上，但不能倒灌为新的 core truth source。

- **规则 2.3：学科层必须依赖公共承载面，而不是改写公共承载面的含义。**
  说明：如果某能力需要把 `attachment`、`anchor`、`query hit` 重新解释成学科私有对象，说明边界设计错误。

- **规则 2.4：防止单一学科绑死内核的默认策略是“晚进入、窄进入、独立进入”。**
  说明：先用独立 capability track 验证，再决定是否需要新的公共面；禁止先把学科模型塞进 core 再讨论是否通用。

### 3. Attachment / Source Reference 通用规则

- **规则 3.1：`attachment` 是未来多学科兼容的主承载面。**
  说明：外部文档、图像、结构文件、谱图文件、实验记录、PDF 及其他学科型材料，首先应被视为 attachment truth 的不同内容形态，而不是新的并列真相系统。

- **规则 3.2：`source reference / anchor` 体系必须保持学科中立。**
  说明：anchor 只表达“可序列化、可重建、可验证的来源定位”，不能内置化学选区、物理对象句柄、生物实体坐标等学科专用选择模型。

- **规则 3.3：学科对象必须挂接在 attachment / source reference 之上，而不是绕开它们自建来源体系。**
  说明：任何正式学科对象都必须能追溯到 note 或 attachment 上的稳定 source reference；不能接受无来源锚点的“语义块”进入正式内核。

- **规则 3.4：attachment-backed 学科能力必须复用公共文档身份键。**
  说明：公共身份键应继续使用稳定 `rel_path` 加相应 revision / anchor basis；不得把 `chem_doc_id`、`bio_entity_doc_id` 之类私有键升级成公共身份主键。

### 4. Metadata 规则

- **规则 4.1：metadata 必须分为通用 metadata 与学科 metadata 两层。**
  说明：通用 metadata 服务于 identity、presence、revision、coarse kind、ready state、diagnostics；学科 metadata 服务于 domain-specific 解释和派生。

- **规则 4.2：学科 metadata 必须命名空间化。**
  说明：学科字段必须以独立 capability 名称和 revision 管理，不能把 `formula`、`peak_count`、`organism`、`gene_symbol` 直接塞进公共 metadata 面。

- **规则 4.3：只有跨学科稳定且对所有宿主都必要的字段，才允许进入公共 metadata 面。**
  说明：例如 `presence`、`content revision`、`coarse attachment kind`、`ready / partial / unavailable` 这类字段可以公共化；学科解释字段不可以。

- **规则 4.4：学科 metadata 的缺失、降级或失败不得破坏通用 surface。**
  说明：即使某个 extractor 失败，attachment identity、search、rebuild、recovery、diagnostics 仍必须保持成立。

### 5. Search / Query 规则

- **规则 5.1：search kernel 只保持跨学科通用检索语义。**
  说明：公共 search contract 只承载文本、路径、标签、反链、attachment、source reference 等通用检索能力，不承载任何单学科检索语法。

- **规则 5.2：学科专用检索能力必须以独立 query contract 扩展。**
  说明：结构检索、谱图检索、公式检索、实体检索等都应作为独立 capability surface 冻结，而不是偷偷扩写公共 search 参数含义。

- **规则 5.3：公共 search contract 不得写死某一学科的过滤、排序或匹配规则。**
  说明：如果某个过滤器只对化学成立，或某种 ranking 只对生物成立，它就不属于公共 search contract。

- **规则 5.4：学科 query 结果必须回落到公共身份体系。**
  说明：即使是学科专用查询，结果也应能稳定映射回 note、attachment、source reference 或独立 capability contract 中定义的可重建对象。

### 6. Public Surface / ABI 规则

- **规则 6.1：公共 ABI 必须保持最小、粗粒度、宿主长期稳定。**
  说明：公共 ABI 只暴露宿主长期依赖且难以替代的能力；优先暴露稳定结果面，不暴露学科内部处理过程。

- **规则 6.2：学科能力不得默认进入公共 ABI。**
  说明：只有当某学科能力已经形成正式 capability surface，且宿主确实需要稳定依赖它时，才允许讨论公共 ABI。

- **规则 6.3：实验性学科能力不得污染既有稳定接口。**
  说明：禁止为了实验能力修改现有公共结构体语义、复用已有字段偷塞学科含义，或让宿主提前依赖未冻结字段。

- **规则 6.4：新增公共 ABI 必须是能力边界清晰、语义加法式的扩展。**
  说明：不得通过重解释旧枚举、改变旧返回语义、或让旧接口对学科状态产生隐式分支来完成扩展。

### 7. Rebuild / Recovery / Diagnostics 规则

- **规则 7.1：任何学科扩展都必须可 rebuild。**
  说明：学科派生状态必须能从文件系统 truth、attachment、source reference、anchor 和冻结 extractor 规则重新构建，不能依赖人工补丁或只存在一次的临时缓存。

- **规则 7.2：任何学科扩展都必须可 recovery。**
  说明：崩溃后重开、watcher 中断、后台 rebuild 中断，都必须能回到一致状态；不能存在“某学科结果一旦丢失就只能手工修复”的正式能力。

- **规则 7.3：任何学科扩展都必须可 diagnostics / export。**
  说明：支持包中必须能导出 capability revision、schema version、extract mode、counts、anomalies、degraded states 和 failure summary。

- **规则 7.4：不可重建、不可恢复、不可诊断的学科能力不得进入正式轨道。**
  说明：这类能力最多只能停留在实验层，不能被描述为 kernel capability。

### 8. Domain Capability Track 规则

- **规则 8.1：新学科能力必须以独立 track 进入，而不是静默渗入 core。**
  说明：每条能力线都要有单独的 scope、non-goals、frozen rules、contract、regression、benchmark 和 diagnostics 说明。

- **规则 8.2：学科能力推进顺序必须分阶段。**
  说明：推荐顺序是：问题边界与非目标冻结、通用承载面复用、metadata / anchor / source reference 规则冻结、query contract 冻结、diagnostics / regression / benchmark 完成、最后才讨论 ABI。

- **规则 8.3：满足治理闭环后，才可称为“正式能力面”。**
  说明：只有当 contract 已冻结、rebuild / recovery 已闭环、diagnostics 可导出、benchmark 有基线、regression 有矩阵、gate 已接入时，才算正式 capability surface。

- **规则 8.4：未满足治理闭环前，一律视为实验能力。**
  说明：实验能力可以存在，但不能要求宿主长期依赖，也不能反向推动 core schema 或公共 ABI 改动。

### 9. UI / Workflow 边界规则

- **规则 9.1：kernel 只负责真相、派生、引用、查询、重建、恢复和诊断。**
  说明：这包括稳定 identity、attachment/source reference、query surface、rebuild / recovery 生命周期和 support bundle 数据。

- **规则 9.2：宿主 / UI / workflow 层负责交互壳层与流程编排。**
  说明：viewer、选区交互、面板状态、拖拽、批处理流程、研究工作流、实验操作壳层都不属于 kernel。

- **规则 9.3：任何需要会话态、视图态或手势态才能成立的对象，都不得写回 kernel truth。**
  说明：kernel anchor 必须独立于 viewer session；不能把用户当前高亮区、临时框选、分页位置、工作台上下文写成内核对象。

- **规则 9.4：学科交互壳层可以消费 kernel surface，但不能把自己反向升级成 kernel 边界。**
  说明：交互便利性不是新增 core object、core ABI 或 core schema 的充分理由。

### 10. Compatibility Governance 规则

- **规则 10.1：`contract revision` 用来冻结宿主可见语义。**
  说明：只要公共字段含义、排序规则、状态语义、结果约束发生变化，就必须通过新的 contract revision 明确表达。

- **规则 10.2：`schema version` 只管理持久化派生状态布局。**
  说明：schema version 不等于产品版本，也不等于 capability 宣传级别；它只回答“当前派生状态如何落盘和迁移”。

- **规则 10.3：`diagnostics revision` 用来冻结支持包导出语义。**
  说明：support bundle 中的字段名、统计口径、异常分类、摘要结构一旦成为外部依赖，就必须通过 diagnostics revision 管理。

- **规则 10.4：`regression` 负责守住语义不回退。**
  说明：凡是已经冻结为 contract 的行为，都必须有对应 regression coverage；没有 regression 的“规则”不算正式规则。

- **规则 10.5：`benchmark` 负责守住能力扩展不破坏 baseline 成本。**
  说明：多学科扩展不能以隐性代价侵蚀 host-stable baseline；新 track 必须证明其成本边界可观测、可回归、可 gate。

- **规则 10.6：`gate` 负责阻止未完成治理闭环的能力冒充正式能力面。**
  说明：没有通过 contract / regression / benchmark / diagnostics gate 的能力，不得进入 release 叙述中的正式 surface。

## C. 多学科扩展准入清单

每条学科能力线在进入 kernel 前，必须逐条回答以下问题：

- [ ] 1. 这条能力线的真相源是什么？它是否仍然建立在文件系统 truth 之上，而不是引入并列真相库？
- [ ] 2. 它是否建立在现有 attachment / source reference / search / diagnostics 之上，而不是绕开它们另起一套？
- [ ] 3. 它是否引入了新的 core truth object？
- [ ] 4. 如果引入，是否已经证明该对象是跨学科通用对象，而不是某一学科的私有语义？
- [ ] 5. 它能否 rebuild？
- [ ] 6. 它能否 recovery？
- [ ] 7. 它能否 diagnostics / export？
- [ ] 8. 它是否需要新的 public ABI？
- [ ] 9. 如果需要，是否已经证明不会污染已有稳定宿主接口？
- [ ] 10. 它是否其实只是 UI / workflow 逻辑，被误塞进 kernel？

## D. 禁止事项清单

以下做法会直接破坏多学科兼容性，必须禁止：

- 禁止把化学 / 物理 / 生物某个具体语义直接焊进 core schema、core enum、core query 或 core diagnostics 结论。
- 禁止把 `molecule`、`reaction`、`equation`、`gene`、`protein`、`cell` 等单学科对象直接升级为 core truth object。
- 禁止把 UI 交互模型、viewer 选区模型、会话态对象或 workflow 壳层写进 kernel。
- 禁止让宿主直接依赖 SQLite 内部表、row id、派生索引细节或内部缓存布局。
- 禁止让某学科能力绕开 attachment / source reference / anchor，自建不可追溯的来源体系。
- 禁止让某学科能力绕开 rebuild / recovery / diagnostics / benchmark / regression / gate 纪律。
- 禁止让不可重建、不可恢复、不可诊断的学科能力进入正式 kernel surface。
- 禁止为了未来兼容而先行做平台化过度抽象、空泛插件化或无实证的总线化设计。
- 禁止把实验性学科能力直接升级成公共 ABI，或让宿主提前依赖未冻结字段。
- 禁止通过重解释旧字段、污染旧结构体、篡改旧 contract 的方式偷渡学科扩展。

## 结论

当前内核的多学科兼容策略不是“提前内建所有学科语义”，而是“冻结跨学科承载面，隔离学科语义层，并要求所有新能力线遵守同一套 rebuild / recovery / diagnostics / contract 治理纪律”。
