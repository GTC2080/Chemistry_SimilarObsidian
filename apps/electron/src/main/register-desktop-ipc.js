const { app, BrowserWindow, dialog, ipcMain } = require("electron");
const { DESKTOP_IPC_CHANNELS } = require("../shared/desktop-contract");

function resolveWindow(event) {
  return BrowserWindow.fromWebContents(event.sender) ?? BrowserWindow.getFocusedWindow() ?? null;
}

function registerDesktopIpc() {
  ipcMain.handle(DESKTOP_IPC_CHANNELS.windowMinimize, (event) => {
    const win = resolveWindow(event);
    if (win && !win.isDestroyed()) {
      win.minimize();
    }
    return { ok: true };
  });

  ipcMain.handle(DESKTOP_IPC_CHANNELS.windowToggleMaximize, (event) => {
    const win = resolveWindow(event);
    if (win && !win.isDestroyed()) {
      if (win.isMaximized()) {
        win.unmaximize();
      } else {
        win.maximize();
      }
    }
    return { ok: true };
  });

  ipcMain.handle(DESKTOP_IPC_CHANNELS.windowClose, (event) => {
    const win = resolveWindow(event);
    if (win && !win.isDestroyed()) {
      win.close();
    }
    return { ok: true };
  });

  ipcMain.handle(DESKTOP_IPC_CHANNELS.dialogPickDirectory, async (event) => {
    const win = resolveWindow(event);
    const result = await dialog.showOpenDialog(win ?? undefined, {
      properties: ["openDirectory"]
    });

    if (result.canceled || !Array.isArray(result.filePaths) || result.filePaths.length === 0) {
      return {
        ok: true,
        data: null
      };
    }

    return {
      ok: true,
      data: result.filePaths[0]
    };
  });

  ipcMain.handle(DESKTOP_IPC_CHANNELS.appGetVersion, () => {
    return {
      ok: true,
      data: app.getVersion()
    };
  });
}

module.exports = {
  registerDesktopIpc
};
