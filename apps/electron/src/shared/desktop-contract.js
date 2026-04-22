const DESKTOP_IPC_CHANNELS = Object.freeze({
  windowMinimize: "desktop/window/minimize",
  windowToggleMaximize: "desktop/window/toggle-maximize",
  windowClose: "desktop/window/close",
  dialogPickDirectory: "desktop/dialog/pick-directory",
  appGetVersion: "desktop/app/get-version"
});

module.exports = {
  DESKTOP_IPC_CHANNELS
};
