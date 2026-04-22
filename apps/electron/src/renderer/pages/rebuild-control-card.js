/**
 * rebuild-control-card.js
 *
 * Rebuild workflow controls.
 */

import { rebuild } from "../services/host-api-client.js";
import { createStateSurface } from "../components/shared/state-surface.js";

export function createRebuildControlCard() {
  const card = document.createElement("div");
  card.className = "rebuild-control-card";
  card.style.cssText = `
    padding: 16px;
    border-radius: 8px;
    background: #fff;
    border: 1px solid #e5e7eb;
    margin-bottom: 16px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 12px;";
  heading.textContent = "Rebuild Index";
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
    padding: 6px 10px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    font-size: 13px;
  `;
  controls.appendChild(timeoutInput);

  const timeoutLabel = document.createElement("span");
  timeoutLabel.style.cssText = "font-size: 12px; color: #6b7280;";
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
      statusArea.style.color = "#7f1d1d";
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
    statusArea.style.color = inFlight ? "#92400e" : "#166534";
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
    padding: 8px 16px;
    border-radius: 6px;
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
    padding: 10px 14px;
    border-radius: 6px;
    background: ${bg};
    color: ${color};
    font-size: 13px;
  `;
  el.textContent = text;
  return el;
}
