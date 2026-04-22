export async function pickDirectory() {
  return (window as any).desktopShell?.dialog?.pickDirectory?.() ?? null;
}

export async function minimizeWindow() {
  return (window as any).desktopShell?.window?.minimize?.();
}

export async function toggleMaximizeWindow() {
  return (window as any).desktopShell?.window?.toggleMaximize?.();
}

export async function closeWindow() {
  return (window as any).desktopShell?.window?.close?.();
}

export async function getDesktopAppVersion() {
  return (window as any).desktopShell?.app?.getVersion?.() ?? null;
}
