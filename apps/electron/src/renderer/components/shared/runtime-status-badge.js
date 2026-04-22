/**
 * runtime-status-badge.js
 *
 * Persistent top-bar component showing runtime health.
 */

export function createRuntimeStatusBadge(runtimeEnvelope) {
  const badge = document.createElement("div");
  badge.className = "runtime-status-badge";
  badge.style.cssText = `
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 12px;
    color: #4b5563;
    white-space: nowrap;
  `;

  let dotColor = "#9ca3af";
  let labelText = "Unknown";

  if (runtimeEnvelope && runtimeEnvelope.ok && runtimeEnvelope.data) {
    const indexState = runtimeEnvelope.data.kernel_runtime?.index_state;
    const attached = runtimeEnvelope.data.kernel_binding?.attached;
    const rebuildInFlight = runtimeEnvelope.data.rebuild?.in_flight;

    if (!attached) {
      dotColor = "#ef4444";
      labelText = "Adapter detached";
    } else if (rebuildInFlight) {
      dotColor = "#f97316";
      labelText = "Rebuilding...";
    } else if (indexState === "ready") {
      dotColor = "#22c55e";
      labelText = "Ready";
    } else if (indexState === "catching_up") {
      dotColor = "#eab308";
      labelText = "Catching up...";
    } else if (indexState === "unavailable") {
      dotColor = "#ef4444";
      labelText = "Unavailable";
    }
  }

  const dot = document.createElement("span");
  dot.style.cssText = `
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: ${dotColor};
    display: inline-block;
    flex-shrink: 0;
  `;
  badge.appendChild(dot);

  const label = document.createElement("span");
  label.textContent = labelText;
  badge.appendChild(label);

  return badge;
}
