/**
 * launcher-shell.js
 *
 * Dark two-column launcher shell inspired by desktop vault launchers.
 * Left rail: recent vaults.
 * Right stage: brand hero + action card.
 */

export function createLauncherShell(opts = {}) {
  const { hostVersion } = opts;

  const shell = document.createElement("div");
  shell.className = "launcher-shell";
  shell.style.cssText = `
    display: flex;
    flex-direction: column;
    height: 100vh;
    min-height: 100vh;
    overflow: hidden;
    background:
      radial-gradient(circle at top right, rgba(122, 92, 255, 0.16), transparent 26%),
      radial-gradient(circle at bottom left, rgba(52, 211, 153, 0.08), transparent 24%),
      linear-gradient(180deg, #171717 0%, #121212 100%);
    font-family: "Segoe UI", "PingFang SC", sans-serif;
    color: #f5f5f5;
  `;

  const topBar = document.createElement("header");
  topBar.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 14px 24px;
    color: #c9c9d1;
    font-size: 13px;
    letter-spacing: 0.02em;
    flex-shrink: 0;
  `;

  const appTitle = document.createElement("div");
  appTitle.style.cssText = "font-size: 15px; font-weight: 600; color: #f3f4f6;";
  appTitle.textContent = "Chemistry Obsidian";
  topBar.appendChild(appTitle);

  const rightWrap = document.createElement("div");
  rightWrap.style.cssText = "display: flex; align-items: center; gap: 14px;";

  if (hostVersion) {
    const versionBadge = document.createElement("span");
    versionBadge.style.cssText = `
      padding: 5px 10px;
      border-radius: 999px;
      background: rgba(255,255,255,0.06);
      border: 1px solid rgba(255,255,255,0.08);
      color: #b9bbca;
      font-size: 12px;
    `;
    versionBadge.textContent = `Host ${hostVersion}`;
    rightWrap.appendChild(versionBadge);
  }

  const statusText = document.createElement("span");
  statusText.style.cssText = "font-size: 12px; color: #8f92a4; text-transform: uppercase; letter-spacing: 0.08em;";
  statusText.textContent = "No vault open";
  rightWrap.appendChild(statusText);

  topBar.appendChild(rightWrap);
  shell.appendChild(topBar);

  const main = document.createElement("main");
  main.style.cssText = `
    flex: 1;
    display: grid;
    grid-template-columns: minmax(280px, 330px) minmax(0, 1fr);
    min-height: 0;
    overflow: hidden;
  `;

  const rail = document.createElement("aside");
  rail.className = "launcher-rail";
  rail.style.cssText = `
    padding: 22px 18px 20px 22px;
    border-right: 1px solid rgba(255,255,255,0.08);
    background: linear-gradient(180deg, rgba(255,255,255,0.03), rgba(255,255,255,0.01));
    overflow: auto;
  `;

  const stage = document.createElement("section");
  stage.className = "launcher-stage";
  stage.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: center;
    min-width: 0;
    padding: 20px 36px 24px;
    overflow: hidden;
  `;

  main.appendChild(rail);
  main.appendChild(stage);
  shell.appendChild(main);

  return { element: shell, rail, stage };
}
