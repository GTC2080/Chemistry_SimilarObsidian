/**
 * launcher-shell.js
 *
 * Vault Launcher layout: centered card with light top bar.
 * Top bar shows only app name + neutral "No vault open" text.
 * No runtime diagnostic badges in launcher mode.
 */

export function createLauncherShell(opts = {}) {
  const { children } = opts;

  const shell = document.createElement("div");
  shell.className = "launcher-shell";
  shell.style.cssText = `
    display: flex;
    flex-direction: column;
    min-height: 100vh;
    background: #f3f4f6;
    font-family: "Segoe UI", "PingFang SC", sans-serif;
    color: #1f2937;
  `;

  // Top bar: very light, no diagnostic badges
  const topBar = document.createElement("header");
  topBar.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 14px 20px;
  `;

  const appTitle = document.createElement("span");
  appTitle.style.cssText = "font-weight: 600; font-size: 15px; color: #374151;";
  appTitle.textContent = "Chemistry Obsidian";
  topBar.appendChild(appTitle);

  const statusText = document.createElement("span");
  statusText.style.cssText = "font-size: 12px; color: #9ca3af;";
  statusText.textContent = "No vault open";
  topBar.appendChild(statusText);

  shell.appendChild(topBar);

  // Main: centered card
  const main = document.createElement("main");
  main.style.cssText = `
    flex: 1;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 24px;
  `;

  const card = document.createElement("div");
  card.className = "launcher-card";
  card.style.cssText = `
    width: 100%;
    max-width: 480px;
    padding: 32px;
    border-radius: 14px;
    background: #fff;
    border: 1px solid #e5e7eb;
    box-shadow: 0 4px 20px rgba(0,0,0,0.06);
  `;

  if (children) {
    card.appendChild(children);
  }

  main.appendChild(card);
  shell.appendChild(main);

  return { element: shell, card };
}
