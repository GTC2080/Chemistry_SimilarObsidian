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
    min-height: 220px;
    padding: 32px;
    border-radius: 22px;
    border: 1px solid rgba(255, 255, 255, 0.06);
    background: linear-gradient(180deg, rgba(35, 34, 41, 0.96), rgba(26, 25, 31, 0.96));
    box-shadow: inset 0 1px 0 rgba(255,255,255,0.03);
    text-align: center;
    color: #c4bddc;
    font-family: inherit;
  `,
  spinner: `
    width: 26px;
    height: 26px;
    border: 3px solid rgba(255,255,255,0.12);
    border-top-color: #8b5cf6;
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
    opacity: 0.72;
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
      padding: 10px 18px;
      border-radius: 10px;
      border: 1px solid rgba(139, 92, 246, 0.45);
      background: rgba(124, 58, 237, 0.18);
      color: #e9ddff;
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
  el.style.cssText = STYLES.surface + " align-items: stretch; background: transparent; border: none; padding: 0; min-height: auto; box-shadow: none;";

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
