import { appShell } from "./app-shell.js";

appShell.init().catch((err) => {
  console.error("App shell init failed:", err);

  const root = document.getElementById("app-root");
  if (!root) {
    return;
  }

  root.innerHTML =
    '<div style="padding:32px;text-align:center;color:#7f1d1d;">Failed to initialize app shell.</div>';
});
