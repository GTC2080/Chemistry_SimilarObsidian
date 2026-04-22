const fs = require("node:fs");
const { resolveBindingCandidates } = require("./native-binding-resolution");

function tryLoadNativeBindingFromResolution(resolution, options = {}) {
  const fileExists = options.fileExists ?? ((candidate) => fs.existsSync(candidate));
  const loadModule = options.loadModule ?? ((candidate) => require(candidate));
  const existingPath = resolution.candidates.find((candidate) => fileExists(candidate));

  if (!existingPath) {
    return {
      ok: false,
      loadState: "not_found",
      runMode: resolution.runMode,
      bindingMechanism: "node_api_addon",
      bindingBasename: resolution.bindingBasename,
      resolvedPath: null,
      candidates: resolution.candidates,
      error: {
        code: "HOST_KERNEL_BINDING_NOT_FOUND",
        message: "Kernel native binding was not found for the current run mode."
      }
    };
  }

  try {
    const binding = loadModule(existingPath);
    const bindingInfo = typeof binding.getBindingInfo === "function"
      ? binding.getBindingInfo()
      : {};

    return {
      ok: true,
      loadState: "loaded",
      runMode: resolution.runMode,
      bindingMechanism: "node_api_addon",
      bindingBasename: resolution.bindingBasename,
      resolvedPath: existingPath,
      candidates: resolution.candidates,
      binding,
      bindingInfo
    };
  } catch (error) {
    return {
      ok: false,
      loadState: "load_failed",
      runMode: resolution.runMode,
      bindingMechanism: "node_api_addon",
      bindingBasename: resolution.bindingBasename,
      resolvedPath: existingPath,
      candidates: resolution.candidates,
      error: {
        code: "HOST_KERNEL_BINDING_LOAD_FAILED",
        message: "Kernel native binding failed to load.",
        details: {
          loadMessage: error instanceof Error ? error.message : String(error)
        }
      }
    };
  }
}

function tryLoadNativeBinding() {
  return tryLoadNativeBindingFromResolution(resolveBindingCandidates());
}

module.exports = {
  tryLoadNativeBindingFromResolution,
  tryLoadNativeBinding
};
