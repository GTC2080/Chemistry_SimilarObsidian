/**
 * host-error-card.js
 *
 * Structured error display from a host envelope.
 */

export function createHostErrorCard(error, opts = {}) {
  const { onRetry } = opts;
  const card = document.createElement("div");
  card.className = "host-error-card";
  card.style.cssText = `
    padding: 16px;
    border-radius: 8px;
    background: #fef2f2;
    border: 1px solid #fecaca;
    color: #7f1d1d;
    font-family: inherit;
  `;

  const code = document.createElement("div");
  code.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 4px;";
  code.textContent = error?.code ? `Error: ${error.code}` : "Error";
  card.appendChild(code);

  if (error?.message) {
    const msg = document.createElement("div");
    msg.style.cssText = "font-size: 13px; margin-bottom: 8px;";
    msg.textContent = error.message;
    card.appendChild(msg);
  }

  if (error?.details && typeof error.details === "object") {
    const details = document.createElement("pre");
    details.style.cssText = `
      font-size: 12px;
      background: rgba(0,0,0,0.04);
      padding: 8px;
      border-radius: 4px;
      overflow: auto;
      margin: 0;
    `;
    details.textContent = JSON.stringify(error.details, null, 2);
    card.appendChild(details);
  }

  if (typeof onRetry === "function") {
    const btn = document.createElement("button");
    btn.textContent = "Retry";
    btn.style.cssText = `
      margin-top: 12px;
      padding: 6px 14px;
      border-radius: 6px;
      border: 1px solid #ef4444;
      background: #ef4444;
      color: #fff;
      cursor: pointer;
      font-size: 13px;
    `;
    btn.addEventListener("click", onRetry);
    card.appendChild(btn);
  }

  return card;
}
