window.addEventListener("DOMContentLoaded", () => {
  const detail = document.getElementById("host-shell-detail");
  const summary = document.getElementById("host-shell-summary");

  async function renderSummary() {
    if (!detail || !summary) {
      return;
    }

    if (!window.hostShell) {
      detail.textContent = "Preload bridge unavailable.";
      return;
    }

    const [bootstrap, runtime, session] = await Promise.all([
      window.hostShell.bootstrap.getInfo(),
      window.hostShell.runtime.getSummary(),
      window.hostShell.session.getStatus()
    ]);

    if (!bootstrap.ok) {
      detail.textContent = "Bootstrap bridge unavailable.";
      summary.textContent = JSON.stringify({ bootstrap, runtime, session }, null, 2);
      return;
    }

    detail.textContent = `Preload bridge ready | Electron ${bootstrap.data.versions.electron} | Node ${bootstrap.data.versions.node}`;
    summary.textContent = JSON.stringify(
      {
        bootstrap,
        runtime,
        session
      },
      null,
      2
    );
  }

  renderSummary().catch(() => {
    if (detail) {
      detail.textContent = "Failed to read host summary.";
    }
  });
});
