/**
 * session-status-card.js
 *
 * Detailed session panel.
 */

export function createSessionStatusCard(sessionEnvelope) {
  const card = document.createElement("div");
  card.className = "session-status-card";
  card.style.cssText = `
    padding: 16px;
    border-radius: 8px;
    background: #fff;
    border: 1px solid #e5e7eb;
    font-size: 13px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; margin-bottom: 10px; font-size: 14px;";
  heading.textContent = "Session";
  card.appendChild(heading);

  if (!sessionEnvelope || !sessionEnvelope.ok) {
    const err = document.createElement("div");
    err.style.color = "#7f1d1d";
    err.textContent = "Unable to read session status.";
    card.appendChild(err);
    return card;
  }

  const data = sessionEnvelope.data;

  const rows = [
    { label: "State", value: data?.state ?? "—" },
    { label: "Vault", value: data?.active_vault_path ?? "—" },
    { label: "Adapter", value: data?.adapter_attached ? "Attached" : "Detached" }
  ];

  for (const row of rows) {
    const wrap = document.createElement("div");
    wrap.style.cssText = "display: flex; justify-content: space-between; padding: 4px 0; border-bottom: 1px solid #f3f4f6;";

    const lbl = document.createElement("span");
    lbl.style.color = "#6b7280";
    lbl.textContent = row.label;
    wrap.appendChild(lbl);

    const val = document.createElement("span");
    val.style.fontWeight = "500";
    val.textContent = row.value;
    wrap.appendChild(val);

    card.appendChild(wrap);
  }

  if (data?.last_error) {
    const errorBlock = document.createElement("div");
    errorBlock.style.cssText = `
      margin-top: 10px;
      padding: 10px;
      border-radius: 6px;
      background: #fef2f2;
      border: 1px solid #fecaca;
      color: #7f1d1d;
    `;

    const errorCode = document.createElement("div");
    errorCode.style.fontWeight = "600";
    errorCode.textContent = data.last_error.code || "Error";
    errorBlock.appendChild(errorCode);

    if (data.last_error.message) {
      const errorMsg = document.createElement("div");
      errorMsg.textContent = data.last_error.message;
      errorBlock.appendChild(errorMsg);
    }

    card.appendChild(errorBlock);
  }

  return card;
}
