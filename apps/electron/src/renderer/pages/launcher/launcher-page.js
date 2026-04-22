/**
 * launcher-page.js
 *
 * Right-stage launcher content:
 * hero block + action card, visually aligned with the desktop launcher reference.
 */

import { addRecentVault } from "./recent-vaults-list.js";

export function createLauncherPage(opts = {}) {
  const { onOpenVault, lastError, isOpening, hostVersion } = opts;

  const page = document.createElement("div");
  page.className = "launcher-page";
  page.style.cssText = `
    width: 100%;
    max-width: 620px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 16px;
  `;

  const hero = document.createElement("div");
  hero.style.cssText = `
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    text-align: center;
  `;

  const brandMark = createBrandMark();
  hero.appendChild(brandMark);

  const title = document.createElement("h1");
  title.style.cssText = "margin: 0; font-size: 42px; line-height: 0.94; font-weight: 700; letter-spacing: -0.045em; color: #ffffff; max-width: 520px; text-wrap: balance;";
  title.textContent = "Chemistry Obsidian";
  hero.appendChild(title);

  const sub = document.createElement("div");
  sub.style.cssText = "font-size: 14px; color: #9aa0b6;";
  sub.textContent = hostVersion ? `版本 ${hostVersion}` : "Electron host baseline";
  hero.appendChild(sub);

  page.appendChild(hero);

  const actionCard = document.createElement("section");
  actionCard.style.cssText = `
    width: 100%;
    max-width: 620px;
    padding: 10px 22px 16px;
    border-radius: 22px;
    background: rgba(255,255,255,0.04);
    border: 1px solid rgba(255,255,255,0.08);
    box-shadow: 0 22px 42px rgba(0,0,0,0.30);
    backdrop-filter: blur(8px);
  `;
  page.appendChild(actionCard);

  const statusArea = document.createElement("div");
  statusArea.style.cssText = "margin-top: 12px;";

  let inputExpanded = false;
  let localNotice = null;

  const createRow = buildActionRow({
    title: "新建仓库",
    description: "在指定文件夹下创建一个新的仓库。",
    actionLabel: "创建",
    accent: "primary",
    disabled: isOpening
  });
  actionCard.appendChild(createRow.row);

  const openRow = buildActionRow({
    title: "打开本地仓库",
    description: "将一个本地文件夹作为仓库在 Host Shell 中打开。",
    actionLabel: isOpening ? "打开中" : "打开",
    accent: "secondary",
    disabled: isOpening
  });
  actionCard.appendChild(openRow.row);

  const hostRow = buildActionRow({
    title: "宿主桥接层",
    description: "当前只启动 Host Shell；打开仓库后进入完整工作区。",
    actionLabel: "就绪",
    accent: "ghost",
    staticAction: true
  });
  hostRow.row.style.borderBottom = "none";
  actionCard.appendChild(hostRow.row);

  const footer = document.createElement("div");
  footer.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    margin-top: 12px;
    padding-top: 14px;
    border-top: 1px solid rgba(255,255,255,0.08);
    color: #8d90a2;
    font-size: 12px;
  `;

  const footerHint = document.createElement("div");
  footerHint.textContent = "左侧可直接打开最近仓库；右侧用于打开新的本地仓库。";
  footer.appendChild(footerHint);

  const footerMode = document.createElement("div");
  footerMode.style.cssText = `
    padding: 6px 10px;
    border-radius: 999px;
    background: rgba(255,255,255,0.04);
    border: 1px solid rgba(255,255,255,0.08);
    color: #b9bbca;
    white-space: nowrap;
  `;
  footerMode.textContent = "Single-vault baseline";
  footer.appendChild(footerMode);

  actionCard.appendChild(footer);
  actionCard.appendChild(statusArea);

  const inputWrap = document.createElement("div");
  inputWrap.style.cssText = `
    display: flex;
    gap: 10px;
    overflow: hidden;
    max-height: 0;
    opacity: 0;
    margin-top: 0;
    transition: max-height 0.25s ease, opacity 0.2s ease, margin 0.2s ease;
  `;

  const pathInput = document.createElement("input");
  pathInput.type = "text";
  pathInput.placeholder = "E:\\Notes\\MyVault";
  pathInput.disabled = isOpening;
  pathInput.style.cssText = `
    flex: 1;
    min-width: 0;
    padding: 10px 12px;
    border-radius: 10px;
    border: 1px solid rgba(255,255,255,0.12);
    background: rgba(255,255,255,0.05);
    color: #f5f5f5;
    font-size: 13px;
    outline: none;
  `;
  inputWrap.appendChild(pathInput);

  const submitBtn = document.createElement("button");
  submitBtn.textContent = isOpening ? "打开中" : "确认打开";
  submitBtn.disabled = isOpening;
  submitBtn.style.cssText = buttonStyle("primary", isOpening);
  inputWrap.appendChild(submitBtn);

  openRow.extra.appendChild(inputWrap);

  renderNotice(lastError ? { tone: "error", error: lastError } : null);

  function expandInput() {
    inputExpanded = true;
    inputWrap.style.maxHeight = "60px";
    inputWrap.style.opacity = "1";
    inputWrap.style.marginTop = "12px";
    pathInput.focus();
  }

  function collapseInput() {
    inputExpanded = false;
    inputWrap.style.maxHeight = "0";
    inputWrap.style.opacity = "0";
    inputWrap.style.marginTop = "0";
  }

  async function submit() {
    const path = pathInput.value.trim();
    if (!path) {
      localNotice = {
        tone: "error",
        error: {
          code: "HOST_INVALID_ARGUMENT",
          message: "请先输入一个有效的 vault 路径。"
        }
      };
      renderNotice(localNotice);
      pathInput.focus();
      return;
    }

    localNotice = null;
    renderNotice(null);

    if (typeof onOpenVault === "function") {
      const result = await onOpenVault(path);
      if (result?.ok) {
        addRecentVault(path);
      }
    }
  }

  createRow.action.addEventListener("click", () => {
    localNotice = {
      tone: "info",
      title: "创建仓库尚未接线",
      message: "当前 baseline 先支持打开本地 vault；创建流程会在后续 host integration 中接入。"
    };
    renderNotice(localNotice);
  });

  openRow.action.addEventListener("click", () => {
    if (!inputExpanded) {
      expandInput();
    } else {
      submit();
    }
  });

  submitBtn.addEventListener("click", submit);
  pathInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") submit();
  });
  pathInput.addEventListener("input", () => {
    if (localNotice && localNotice.tone === "error") {
      localNotice = null;
      renderNotice(null);
      return;
    }

    if (lastError) {
      renderNotice(null);
    }
  });

  if (isOpening) {
    const overlay = document.createElement("div");
    overlay.style.cssText = `
      position: absolute;
      inset: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(17,17,17,0.54);
      backdrop-filter: blur(2px);
      border-radius: 24px;
      font-size: 14px;
      color: #f5f5f5;
      z-index: 1;
    `;
    overlay.textContent = "Opening vault...";
    actionCard.style.position = "relative";
    actionCard.appendChild(overlay);
  }

  return page;

  function renderNotice(notice) {
    statusArea.innerHTML = "";

    if (!notice) {
      return;
    }

    const block = document.createElement("div");
    const palette = notice.tone === "info"
      ? {
          bg: "rgba(124, 92, 255, 0.14)",
          border: "rgba(147, 118, 255, 0.28)",
          text: "#ddd6fe"
        }
      : {
          bg: "rgba(127, 29, 29, 0.24)",
          border: "rgba(248, 113, 113, 0.28)",
          text: "#fecaca"
        };

    block.style.cssText = `
      padding: 14px 16px;
      border-radius: 14px;
      background: ${palette.bg};
      border: 1px solid ${palette.border};
      color: ${palette.text};
      font-size: 12px;
      line-height: 1.55;
    `;

    const titleLine = document.createElement("div");
    titleLine.style.cssText = "font-weight: 600; margin-bottom: 4px;";
    titleLine.textContent = notice.title || notice.error?.code || "Notice";
    block.appendChild(titleLine);

    const detailLine = document.createElement("div");
    detailLine.textContent = notice.message || notice.error?.message || "";
    block.appendChild(detailLine);

    statusArea.appendChild(block);
  }
}

function buildActionRow(opts = {}) {
  const {
    title,
    description,
    actionLabel,
    accent = "secondary",
    staticAction = false,
    disabled = false
  } = opts;

  const row = document.createElement("div");
  row.style.cssText = `
    padding: 14px 0;
    border-bottom: 1px solid rgba(255,255,255,0.08);
  `;

  const top = document.createElement("div");
  top.style.cssText = "display: flex; align-items: flex-start; justify-content: space-between; gap: 14px;";

  const textWrap = document.createElement("div");
  textWrap.style.cssText = "min-width: 0; flex: 1;";

  const titleEl = document.createElement("div");
  titleEl.style.cssText = "font-size: 16px; font-weight: 600; color: #ffffff; margin-bottom: 5px;";
  titleEl.textContent = title;
  textWrap.appendChild(titleEl);

  const descEl = document.createElement("div");
  descEl.style.cssText = "font-size: 13px; line-height: 1.5; color: #9da1b4;";
  descEl.textContent = description;
  textWrap.appendChild(descEl);

  top.appendChild(textWrap);

  const action = document.createElement("button");
  action.textContent = actionLabel;
  action.style.cssText = buttonStyle(accent, disabled, staticAction);
  action.disabled = staticAction || disabled;
  top.appendChild(action);

  row.appendChild(top);

  const extra = document.createElement("div");
  row.appendChild(extra);

  return { row, action, extra };
}

function buttonStyle(accent, disabled, staticAction = false) {
  const common = `
    min-width: 108px;
    padding: 10px 18px;
    border-radius: 10px;
    border: 1px solid transparent;
    font-size: 14px;
    cursor: ${staticAction ? "default" : "pointer"};
    transition: transform 0.15s ease, opacity 0.15s ease, background 0.15s ease;
    opacity: ${disabled ? "0.65" : "1"};
    flex-shrink: 0;
  `;

  if (accent === "primary") {
    return common + `
      background: linear-gradient(135deg, #8b5cf6 0%, #6d4bf3 100%);
      color: #fff;
      box-shadow: 0 12px 26px rgba(109, 75, 243, 0.28);
    `;
  }

  if (accent === "ghost") {
    return common + `
      background: rgba(255,255,255,0.05);
      border-color: rgba(255,255,255,0.10);
      color: #d1d5db;
    `;
  }

  return common + `
    background: rgba(255,255,255,0.08);
    border-color: rgba(255,255,255,0.10);
    color: #f3f4f6;
  `;
}

function createBrandMark() {
  const wrap = document.createElement("div");
  wrap.style.cssText = `
    width: 96px;
    height: 96px;
    position: relative;
    filter: drop-shadow(0 16px 24px rgba(71, 29, 190, 0.30));
  `;

  const gem = document.createElement("div");
  gem.style.cssText = `
    position: absolute;
    inset: 0;
    border-radius: 38% 30% 42% 34% / 28% 42% 32% 46%;
    background: conic-gradient(from 220deg, #4722b8 0deg, #6d4bf3 82deg, #b79dff 156deg, #8f71ff 224deg, #4d26c6 308deg, #4722b8 360deg);
    transform: rotate(14deg);
  `;
  wrap.appendChild(gem);

  const shardA = document.createElement("div");
  shardA.style.cssText = `
    position: absolute;
    left: 16px;
    top: 8px;
    width: 38px;
    height: 54px;
    border-radius: 42% 28% 38% 22%;
    background: linear-gradient(180deg, rgba(255,255,255,0.56), rgba(255,255,255,0.10));
    transform: rotate(18deg);
    mix-blend-mode: screen;
  `;
  wrap.appendChild(shardA);

  const shardB = document.createElement("div");
  shardB.style.cssText = `
    position: absolute;
    right: 14px;
    bottom: 14px;
    width: 34px;
    height: 36px;
    border-radius: 30% 44% 38% 48%;
    background: linear-gradient(180deg, rgba(22, 12, 68, 0.12), rgba(28, 15, 88, 0.82));
    transform: rotate(-22deg);
  `;
  wrap.appendChild(shardB);

  return wrap;
}
