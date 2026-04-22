/**
 * rebuild-control-card.js
 *
 * Rebuild workflow controls.
 */

import { rebuild } from "../../services/host-api-client.js";
import { createStateSurface } from "../../components/shared/state-surface.js";

export function createRebuildControlCard() {
  const card = document.createElement("div");
  card.className = "rebuild-control-card";
  card.style.cssText = `
    padding: 18px;
    border-radius: 20px;
    background: rgba(29, 28, 35, 0.94);
    border: 1px solid rgba(255,255,255,0.06);
    margin-bottom: 16px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 12px; color:#f5f3ff;";
  heading.textContent = "重建索引";
  card.appendChild(heading);

  const statusArea = document.createElement("div");
  statusArea.style.cssText = "margin-bottom: 12px; font-size: 13px;";
  card.appendChild(statusArea);

  const controls = document.createElement("div");
  controls.style.cssText = "display: flex; gap: 8px; align-items: center; flex-wrap: wrap;";

  const startBtn = document.createElement("button");
  startBtn.textContent = "Start Rebuild";
  startBtn.style.cssText = buttonStyle("#2563eb");
  controls.appendChild(startBtn);

  const waitBtn = document.createElement("button");
  waitBtn.textContent = "Wait for Rebuild";
  waitBtn.style.cssText = buttonStyle("#059669");
  controls.appendChild(waitBtn);

  const timeoutInput = document.createElement("input");
  timeoutInput.type = "number";
  timeoutInput.value = "30000";
  timeoutInput.min = "1000";
  timeoutInput.step = "1000";
  timeoutInput.style.cssText = `
    width: 100px;
    padding: 8px 10px;
    border-radius: 10px;
    border: 1px solid rgba(255,255,255,0.08);
    font-size: 13px;
    background: rgba(255,255,255,0.04);
    color: #f5f3ff;
  `;
  controls.appendChild(timeoutInput);

  const timeoutLabel = document.createElement("span");
  timeoutLabel.style.cssText = "font-size: 12px; color: #6b7280;";
  timeoutLabel.style.color = "#968ead";
  timeoutLabel.textContent = "ms";
  controls.appendChild(timeoutLabel);

  card.appendChild(controls);

  const resultArea = document.createElement("div");
  resultArea.style.cssText = "margin-top: 10px;";
  card.appendChild(resultArea);

  async function refreshStatus() {
    const env = await rebuild.getStatus("app-rebuild-status");
    if (!env.ok) {
      statusArea.textContent = "Unable to read rebuild status.";
      statusArea.style.color = "#fca5a5";
      return;
    }

    const data = env.data;
    const inFlight = data?.status?.inFlight ?? false;
    startBtn.disabled = inFlight;
    startBtn.style.opacity = inFlight ? "0.5" : "1";

    const parts = [];
    if (inFlight) parts.push("Rebuild in flight");
    else parts.push("No rebuild in flight");

    if (data?.status?.hasLastResult) {
      parts.push(`Last: ${data.status.lastResultCode}`);
    }
    if (data?.status?.indexState) {
      parts.push(`Index: ${data.status.indexState}`);
    }

    statusArea.textContent = parts.join(" | ");
    statusArea.style.color = inFlight ? "#fde68a" : "#86efac";
  }

  startBtn.addEventListener("click", async () => {
    resultArea.innerHTML = "";
    const env = await rebuild.start("app-rebuild-start");
    if (!env.ok) {
      if (env.error?.code === "HOST_REBUILD_ALREADY_RUNNING") {
        resultArea.appendChild(banner("Rebuild is already in progress.", "#fef9c3", "#854d0e"));
      } else {
        resultArea.appendChild(createStateSurface("error", { error: env.error }));
      }
    } else {
      resultArea.appendChild(banner("Rebuild started.", "#dcfce7", "#166534"));
    }
    await refreshStatus();
  });

  waitBtn.addEventListener("click", async () => {
    const timeoutMs = parseInt(timeoutInput.value, 10) || 30000;
    resultArea.innerHTML = "";
    resultArea.appendChild(createStateSurface("loading", { loadingLabel: "Waiting for rebuild..." }));

    const env = await rebuild.wait({ timeoutMs }, "app-rebuild-wait");
    resultArea.innerHTML = "";

    if (!env.ok) {
      resultArea.appendChild(createStateSurface("error", { error: env.error }));
    } else {
      const result = env.data?.result ?? "completed";
      if (result === "timeout") {
        resultArea.appendChild(banner("Rebuild did not complete within timeout. Check status.", "#fef9c3", "#854d0e"));
      } else {
        resultArea.appendChild(banner(`Rebuild completed.`, "#dcfce7", "#166534"));
      }
    }
    await refreshStatus();
  });

  refreshStatus();
  return card;
}

function buttonStyle(bg) {
  return `
    padding: 10px 18px;
    border-radius: 12px;
    border: none;
    background: ${bg};
    color: #fff;
    cursor: pointer;
    font-size: 13px;
  `;
}

function banner(text, bg, color) {
  const el = document.createElement("div");
  el.style.cssText = `
    padding: 12px 14px;
    border-radius: 14px;
    background: ${bg === "#dcfce7" ? "rgba(22, 101, 52, 0.32)" : "rgba(120, 53, 15, 0.34)"};
    border: 1px solid ${bg === "#dcfce7" ? "rgba(34,197,94,0.22)" : "rgba(250,204,21,0.2)"};
    color: ${bg === "#dcfce7" ? "#86efac" : "#fde68a"};
    font-size: 13px;
  `;
  el.textContent = text;
  return el;
}
