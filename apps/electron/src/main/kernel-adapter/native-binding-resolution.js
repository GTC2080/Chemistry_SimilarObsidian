const path = require("node:path");
const { app } = require("electron");

const BINDING_BASENAME = "kernel_host_binding.node";

function resolveRunMode(options = {}) {
  if (options.packaged === true) {
    return "packaged";
  }

  if (options.packaged === false) {
    return "dev";
  }

  return app && app.isPackaged ? "packaged" : "dev";
}

function resolveAppPath(options = {}) {
  if (typeof options.appPath === "string" && options.appPath.trim()) {
    return path.resolve(options.appPath);
  }

  if (app && typeof app.getAppPath === "function") {
    return path.resolve(app.getAppPath());
  }

  return path.resolve(process.cwd());
}

function resolveResourcesPath(options = {}, appPath) {
  if (typeof options.resourcesPath === "string" && options.resourcesPath.trim()) {
    return path.resolve(options.resourcesPath);
  }

  if (typeof process.resourcesPath === "string" && process.resourcesPath) {
    return path.resolve(process.resourcesPath);
  }

  return path.resolve(appPath, "dist", "resources");
}

function getDevBindingCandidates(appPath) {
  return [
    path.resolve(appPath, "build", "Debug", BINDING_BASENAME),
    path.resolve(appPath, "build", "Release", BINDING_BASENAME),
    path.resolve(appPath, "build", "default", BINDING_BASENAME)
  ];
}

function getPackagedBindingCandidates(resourcesPath) {
  return [
    path.resolve(resourcesPath, "native", BINDING_BASENAME),
    path.resolve(resourcesPath, "app.asar.unpacked", "native", BINDING_BASENAME)
  ];
}

function resolveBindingCandidates(options = {}) {
  const runMode = resolveRunMode(options);
  const appPath = resolveAppPath(options);
  const resourcesPath = resolveResourcesPath(options, appPath);

  return {
    runMode,
    bindingBasename: BINDING_BASENAME,
    appPath,
    resourcesPath,
    candidates: runMode === "packaged"
      ? getPackagedBindingCandidates(resourcesPath)
      : getDevBindingCandidates(appPath)
  };
}

module.exports = {
  BINDING_BASENAME,
  resolveBindingCandidates
};
