/**
 * state-surface.js
 *
 * Unified loading / empty / unavailable / error surface.
 * These four states are mutually exclusive in presentation logic.
 */

import { createHostErrorCard } from "./host-error-card.js";

const STYLES = {
  surface: `
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 32px;
    text-align: center;
    color: #4b5563;
    font-family: inherit;
  `,
  spinner: `
    width: 24px;
    height: 24px;
    border: 3px solid #e5e7eb;
    border-top-color: #3b82f6;
    border-radius: 50%;
    animation: spin 1s linear infinite;
    margin-bottom: 12px;
  `,
  label: `
    font-size: 14px;
    font-weight: 500;
  `,
  icon: `
    font-size: 32px;
    margin-bottom: 8px;
    opacity: 0.6;
  `
};

function injectSpinnerKeyframes() {
  if (document.getElementById("renderer-spin-keyframes")) return;
  const style = document.createElement("style");
  style.id = "renderer-spin-keyframes";
  style.textContent = `@keyframes spin { to { transform: rotate(360deg); } }`;
  document.head.appendChild(style);
}

export function createLoadingSurface(label = "Loading...") {
  injectSpinnerKeyframes();
  const el = document.createElement("div");
  el.className = "state-surface state-surface--loading";
  el.style.cssText = STYLES.surface;

  const spinner = document.createElement("div");
  spinner.style.cssText = STYLES.spinner;
  el.appendChild(spinner);

  const text = document.createElement("div");
  text.style.cssText = STYLES.label;
  text.textContent = label;
  el.appendChild(text);

  return el;
}

export function createEmptySurface(message = "No results.") {
  const el = document.createElement("div");
  el.className = "state-surface state-surface--empty";
  el.style.cssText = STYLES.surface;

  const icon = document.createElement("div");
  icon.style.cssText = STYLES.icon;
  icon.textContent = "∅";
  el.appendChild(icon);

  const text = document.createElement("div");
  text.style.cssText = STYLES.label;
  text.textContent = message;
  el.appendChild(text);

  return el;
}

export function createUnavailableSurface(opts = {}) {
  const { message = "Host unavailable.", onRetry } = opts;
  const el = document.createElement("div");
  el.className = "state-surface state-surface--unavailable";
  el.style.cssText = STYLES.surface;

  const icon = document.createElement("div");
  icon.style.cssText = STYLES.icon;
  icon.textContent = "⚠";
  el.appendChild(icon);

  const text = document.createElement("div");
  text.style.cssText = `${STYLES.label} margin-bottom: 12px;`;
  text.textContent = message;
  el.appendChild(text);

  if (typeof onRetry === "function") {
    const btn = document.createElement("button");
    btn.textContent = "Retry";
    btn.style.cssText = `
      padding: 8px 18px;
      border-radius: 6px;
      border: 1px solid #6b7280;
      background: #fff;
      color: #374151;
      cursor: pointer;
      font-size: 13px;
    `;
    btn.addEventListener("click", onRetry);
    el.appendChild(btn);
  }

  return el;
}

export function createErrorSurface(error, opts = {}) {
  const el = document.createElement("div");
  el.className = "state-surface state-surface--error";
  el.style.cssText = STYLES.surface + " align-items: stretch;";

  const card = createHostErrorCard(error, opts);
  el.appendChild(card);

  return el;
}

/**
 * Render the correct surface for an envelope classification.
 * @param {"loading" | "empty" | "unavailable" | "error" | "content"} classification
 * @param {object} opts
 * @returns {HTMLElement}
 */
export function createStateSurface(classification, opts = {}) {
  switch (classification) {
    case "loading":
      return createLoadingSurface(opts.loadingLabel);
    case "empty":
      return createEmptySurface(opts.emptyMessage);
    case "unavailable":
      return createUnavailableSurface(opts);
    case "error":
      return createErrorSurface(opts.error, opts);
    default:
      // content: return an empty placeholder; caller should render content
      return document.createElement("div");
  }
}
