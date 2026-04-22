/**
 * workspace-home-page.js
 *
 * Default workspace landing view when Files is selected.
 */

export function createWorkspaceHomePage(opts = {}) {
  const { vaultPath, runtimeEnvelope } = opts;

  const page = document.createElement("div");
  page.className = "workspace-home-page";
  page.style.cssText = `
    min-height: calc(100vh - 150px);
    display: grid;
    grid-template-columns: minmax(260px, 300px) minmax(0, 1fr);
    gap: 18px;
    align-items: start;
  `;

  const recentPane = document.createElement("section");
  recentPane.style.cssText = `
    min-height: 100%;
    padding: 18px 16px;
    border-radius: 22px;
    border: 1px solid rgba(255,255,255,0.06);
    background: linear-gradient(180deg, rgba(32,31,38,0.96), rgba(24,23,29,0.96));
    box-shadow: inset 0 1px 0 rgba(255,255,255,0.04);
  `;

  recentPane.appendChild(sectionLabel("Recent"));
  recentPane.appendChild(fileStub("Workspace Home.md", "当前内容中心占位页", true));
  recentPane.appendChild(fileStub("Welcome.md", "默认欢迎笔记占位", false));
  recentPane.appendChild(fileStub("Scratchpad.md", "临时记录占位", false));
  recentPane.appendChild(fileStub("Reading Notes.md", "阅读笔记占位", false));

  const vaultMeta = document.createElement("div");
  vaultMeta.style.cssText = `
    margin-top: 18px;
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;
  vaultMeta.innerHTML = `
    <div style="font-size:11px;letter-spacing:0.16em;text-transform:uppercase;color:#8b83a2;">Vault</div>
    <div style="margin-top:8px;font-size:12px;color:#d9d2ee;line-height:1.7;word-break:break-word;">${escapeHtml(vaultPath || "No active vault path")}</div>
  `;
  recentPane.appendChild(vaultMeta);

  const editorPane = document.createElement("article");
  editorPane.style.cssText = `
    min-height: calc(100vh - 190px);
    padding: 26px 30px 34px;
    border-radius: 22px;
    border: 1px solid rgba(255,255,255,0.06);
    background:
      radial-gradient(circle at top center, rgba(139,92,246,0.08), transparent 36%),
      linear-gradient(180deg, rgba(28,27,34,0.98), rgba(20,19,25,0.98));
    box-shadow: inset 0 1px 0 rgba(255,255,255,0.04);
  `;

  const title = document.createElement("div");
  title.style.cssText = "font-size: 30px; font-weight: 700; color: #faf7ff; letter-spacing: -0.03em;";
  title.textContent = "Workspace Home";
  editorPane.appendChild(title);

  const meta = document.createElement("div");
  meta.style.cssText = "margin-top: 8px; font-size: 12px; color: #938ca9;";
  meta.textContent = buildRuntimeLine(runtimeEnvelope);
  editorPane.appendChild(meta);

  const body = document.createElement("div");
  body.style.cssText = "margin-top: 26px; display: grid; gap: 18px;";

  body.appendChild(noteParagraph(
    "当前 Files 页已经从“功能首页”退回为内容中心占位区。默认主区展示的是一张笔记式工作台，而不是 Search / Attachments / Chemistry / Diagnostics 的大按钮入口。"
  ));
  body.appendChild(noteParagraph(
    "左侧 Sidebar 继续承担工作区导航职责。Search、Attachments、Chemistry、Diagnostics 仍然可用，但它们现在是工具面，不再抢占首页主舞台。"
  ));

  const callout = document.createElement("div");
  callout.style.cssText = `
    padding: 16px 18px;
    border-radius: 18px;
    border: 1px solid rgba(139,92,246,0.2);
    background: rgba(124,58,237,0.08);
    color: #ddd6fe;
    line-height: 1.7;
    font-size: 13px;
  `;
  callout.innerHTML = `
    <div style="font-size:11px;letter-spacing:0.16em;text-transform:uppercase;color:#b7a8ff;margin-bottom:8px;">Content placeholder</div>
    <div>当前还没有正式 Files host surface，所以这里维持“内容占位页”语义，而不是在 Renderer 层硬猜文件树。</div>
  `;
  body.appendChild(callout);

  const checklist = document.createElement("div");
  checklist.style.cssText = `
    margin-top: 4px;
    padding-left: 16px;
    color: #c8c1df;
    font-size: 13px;
    line-height: 1.8;
  `;
  checklist.innerHTML = `
    <div>• Launcher 负责进入 Vault</div>
    <div>• Workspace 默认落在 Files</div>
    <div>• 工具页从侧边栏进入</div>
    <div>• 右侧面板已降级为低频上下文栏</div>
  `;
  body.appendChild(checklist);

  editorPane.appendChild(body);

  page.appendChild(recentPane);
  page.appendChild(editorPane);
  return page;
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}

function sectionLabel(text) {
  const el = document.createElement("div");
  el.style.cssText = `
    margin-bottom: 12px;
    font-size: 11px;
    letter-spacing: 0.2em;
    text-transform: uppercase;
    color: #938ca9;
  `;
  el.textContent = text;
  return el;
}

function fileStub(title, subtitle, active) {
  const card = document.createElement("div");
  card.style.cssText = `
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 14px;
    margin-bottom: 8px;
    border-radius: 16px;
    border: 1px solid ${active ? "rgba(139,92,246,0.28)" : "rgba(255,255,255,0.04)"};
    background: ${active ? "linear-gradient(180deg, rgba(124,58,237,0.16), rgba(124,58,237,0.06))" : "rgba(255,255,255,0.03)"};
    color: #f5f3ff;
  `;
  card.innerHTML = `
    <div style="width:34px;height:34px;border-radius:12px;display:grid;place-items:center;background:rgba(255,255,255,0.04);color:#c9c0ef;">✦</div>
    <div style="min-width:0;">
      <div style="font-size:14px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">${escapeHtml(title)}</div>
      <div style="font-size:12px;color:#968ead;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">${escapeHtml(subtitle)}</div>
    </div>
  `;
  return card;
}

function noteParagraph(text) {
  const p = document.createElement("p");
  p.style.cssText = "margin: 0; font-size: 15px; line-height: 1.9; color: #d8d1ec;";
  p.textContent = text;
  return p;
}

function buildRuntimeLine(runtimeEnvelope) {
  const runtime = runtimeEnvelope?.ok ? runtimeEnvelope.data : null;
  const state = runtime?.kernel_runtime?.index_state ?? "unknown";
  const session = runtime?.kernel_runtime?.session_state ?? "unknown";
  return `session=${session} · index=${state}`;
}
