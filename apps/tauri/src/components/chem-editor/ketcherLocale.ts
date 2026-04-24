/**
 * Ketcher 汉化模块
 * Ketcher 没有内置 i18n，通过 DOM 层面的文本替换实现中文化
 */

/** 英文 → 中文翻译映射 */
const ZH_CN_MAP: Record<string, string> = {
  // ── 顶部工具栏 ──
  "Undo": "撤销",
  "Redo": "重做",
  "Cut": "剪切",
  "Copy": "复制",
  "Copy to clipboard": "复制到剪贴板",
  "Paste": "粘贴",
  "Open...": "打开…",
  "Open": "打开",
  "Open from File": "从文件打开",
  "Open structure": "打开结构",
  "Save": "保存",
  "Save as...": "另存为…",
  "Save Structure": "保存结构",
  "Save to File": "保存到文件",
  "Clear Canvas": "清除画布",
  "Zoom In": "放大",
  "Zoom Out": "缩小",
  "Zoom 100%": "缩放 100%",
  "Fullscreen mode": "全屏模式",
  "Settings": "设置",
  "Help": "帮助",
  "About": "关于",
  "Layout": "排版",
  "Clean Up": "整理",
  "Aromatize": "芳构化",
  "Dearomatize": "去芳构化",
  "Calculate CIP": "计算 CIP",
  "Structure Check": "结构检查",
  "Calculated Values": "计算值",
  "Check": "检查",
  "Miew": "3D 查看器",
  "3D Viewer": "3D 查看器",

  // ── 左侧工具栏 ──
  "Hand Tool": "手形工具",
  "Selection Tool": "选择工具",
  "Selection tool": "选择工具",
  "Rectangle Selection": "矩形选择",
  "Lasso Selection": "套索选择",
  "Fragment Selection": "片段选择",
  "Select All": "全选",
  "Deselect All": "取消全选",
  "Erase": "擦除",
  "Eraser": "橡皮擦",

  // ── 键类型 ──
  "Single Bond": "单键",
  "Double Bond": "双键",
  "Triple Bond": "三键",
  "Any Bond": "任意键",
  "Aromatic Bond": "芳香键",
  "Single Up": "单键（上楔）",
  "Single Down": "单键（下楔）",
  "Single Up/Down": "单键（上下楔）",
  "Double Cis/Trans": "双键（顺/反）",
  "Bond": "键",
  "Bond Properties": "键属性",
  "Bond type": "键类型",

  // ── 链与模板 ──
  "Chain": "碳链",
  "Chain Tool": "碳链工具",
  "Template": "模板",
  "Template Library": "模板库",
  "Template library": "模板库",
  "Functional Groups": "官能团",
  "Salts and Solvents": "盐和溶剂",

  // ── 电荷与变换 ──
  "Charge Plus": "正电荷",
  "Charge Minus": "负电荷",
  "Rotate Tool": "旋转工具",
  "Flip Horizontally": "水平翻转",
  "Flip Vertically": "垂直翻转",

  // ── 原子与基团 ──
  "Atom": "原子",
  "Atom Properties": "原子属性",
  "Extended Table": "扩展元素表",
  "R-Group": "R-基团",
  "R-Group Logic Condition": "R-基团逻辑条件",
  "S-Group Properties": "S-基团属性",
  "S-Group": "S-基团",
  "Attachment Points": "连接点",

  // ── 反应 ──
  "Reaction Arrow": "反应箭头",
  "Reaction Auto-Mapping": "反应自动映射",
  "Reaction Mapping": "反应映射",
  "Plus": "加号",
  "Arrow": "箭头",

  // ── 文本与标签 ──
  "Text": "文本",
  "Text Editor": "文本编辑器",
  "Label Edit": "标签编辑",
  "Name": "名称",
  "Code": "代码",
  "Type": "类型",

  // ── 增强立体化学 ──
  "Enhanced Stereochemistry": "增强立体化学",

  // ── 对话框 ──
  "Cancel": "取消",
  "OK": "确定",
  "Apply": "应用",
  "Close": "关闭",
  "Reset": "重置",
  "Import": "导入",
  "Export": "导出",
  "Delete": "删除",
  "Add": "添加",
  "Remove": "移除",

  // ── 形状 (底部工具栏) ──
  "Shape Ellipse": "椭圆",
  "Shape Rectangle": "矩形",
  "Shape Line": "直线",

  // ── 图片导入 ──
  "Import Structure from Image": "从图片导入结构",

  // ── 氢 ──
  "Add/Remove explicit hydrogens": "添加/移除显式氢",

  // ── 其他 ──
  "Select": "选择",
  "Selected": "已选择",
  "symbols": "符号",
  "Confirm type change": "确认类型更改",
};

// 预构建按长度降序排列的前缀匹配列表（避免每次遍历全表）
const PREFIX_ENTRIES = Object.entries(ZH_CN_MAP)
  .sort((a, b) => b[0].length - a[0].length);

/**
 * 翻译单个字符串
 */
function translateText(text: string): string | null {
  const trimmed = text.trim();
  if (!trimmed) return null;

  // 精确匹配（O(1)）
  const exact = ZH_CN_MAP[trimmed];
  if (exact) return exact;

  // 前缀匹配（处理 "Help (F1)" → "帮助 (F1)" 等情况）
  for (const [en, zh] of PREFIX_ENTRIES) {
    if (trimmed.length > en.length && trimmed.startsWith(en)) {
      const after = trimmed[en.length];
      if (after === " " || after === "(") {
        return zh + trimmed.slice(en.length);
      }
    }
  }

  return null;
}

/** 已翻译节点标记（WeakSet 比 data-attribute 更轻，不触发 attribute mutation） */
const translatedNodes = new WeakSet<Node>();

/**
 * 扫描容器内所有元素，翻译 title 属性和文本内容
 */
function translateContainer(container: HTMLElement): void {
  // 翻译 title 属性（tooltips）
  const titled = container.querySelectorAll("[title]:not([data-ketcher-i18n])");
  for (let i = 0; i < titled.length; i++) {
    const el = titled[i];
    const original = el.getAttribute("title") ?? "";
    const translated = translateText(original);
    if (translated) {
      el.setAttribute("title", translated);
      el.setAttribute("data-ketcher-i18n", original);
    }
  }

  // 翻译按钮和标签中的纯文本节点
  const textElements = container.querySelectorAll(
    "button, span, label, div[class*='title'], p, h2, h3, h4"
  );
  for (let i = 0; i < textElements.length; i++) {
    const el = textElements[i];
    for (const child of el.childNodes) {
      if (child.nodeType === Node.TEXT_NODE && !translatedNodes.has(child)) {
        const text = child.textContent ?? "";
        const translated = translateText(text);
        if (translated) {
          child.textContent = translated;
          translatedNodes.add(child);
        }
      }
    }
  }
}

/**
 * 启动 Ketcher 汉化监听器
 * @param container Ketcher 所在的 DOM 容器
 * @returns 清理函数
 */
export function startKetcherLocale(container: HTMLElement): () => void {
  // 首次翻译
  translateContainer(container);

  // MutationObserver 监听 DOM 变化并翻译新增内容
  // 使用微任务合并：将连续的 mutations 在同一帧内批量处理
  let pendingNodes: HTMLElement[] = [];
  let pendingFrame = 0;

  function flushPending() {
    pendingFrame = 0;
    const nodes = pendingNodes;
    pendingNodes = [];
    for (const node of nodes) {
      if (node.isConnected) {
        translateContainer(node);
      }
    }
  }

  const observer = new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      if (mutation.type === "childList") {
        for (const node of mutation.addedNodes) {
          if (node instanceof HTMLElement) {
            pendingNodes.push(node);
          }
        }
      } else if (mutation.type === "attributes") {
        const el = mutation.target as HTMLElement;
        if (
          mutation.attributeName === "title" &&
          !el.hasAttribute("data-ketcher-i18n")
        ) {
          const title = el.getAttribute("title") ?? "";
          const translated = translateText(title);
          if (translated) {
            el.setAttribute("title", translated);
            el.setAttribute("data-ketcher-i18n", title);
          }
        }
      }
    }

    // 合并到下一帧处理，避免在 mutation callback 中同步触发更多 mutations
    if (pendingNodes.length > 0 && !pendingFrame) {
      pendingFrame = requestAnimationFrame(flushPending);
    }
  });

  observer.observe(container, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ["title"],
  });

  // 延迟再翻译一次（Ketcher 渲染可能有延迟）
  const timer = setTimeout(() => translateContainer(container), 1500);

  return () => {
    observer.disconnect();
    clearTimeout(timer);
    if (pendingFrame) cancelAnimationFrame(pendingFrame);
  };
}
