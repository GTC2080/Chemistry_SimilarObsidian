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
    padding: 18px;
    border-radius: 18px;
    background: linear-gradient(180deg, rgba(61, 19, 32, 0.92), rgba(42, 15, 25, 0.92));
    border: 1px solid rgba(248, 113, 113, 0.35);
    color: #fecaca;
    font-family: inherit;
    box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.04);
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
      background: rgba(0,0,0,0.22);
      border: 1px solid rgba(255,255,255,0.06);
      padding: 10px;
      border-radius: 10px;
      overflow: auto;
      margin: 0;
      color: #fde68a;
    `;
    details.textContent = JSON.stringify(error.details, null, 2);
    card.appendChild(details);
  }

  if (typeof onRetry === "function") {
    const btn = document.createElement("button");
    btn.textContent = "Retry";
    btn.style.cssText = `
      margin-top: 12px;
      padding: 9px 16px;
      border-radius: 10px;
      border: 1px solid rgba(248, 113, 113, 0.55);
      background: linear-gradient(180deg, #ef4444, #dc2626);
      color: #fff7f7;
      cursor: pointer;
      font-size: 13px;
    `;
    btn.addEventListener("click", onRetry);
    card.appendChild(btn);
  }

  return card;
}
