const test = require("node:test");
const assert = require("node:assert/strict");

const {
  tryLoadNativeBindingFromResolution
} = require("../../src/main/kernel-adapter/native-binding-loader");

function createResolution(overrides = {}) {
  return {
    runMode: "packaged",
    bindingBasename: "kernel_host_binding.node",
    candidates: [
      "E:/packaged/resources/native/kernel_host_binding.node",
      "E:/packaged/resources/app.asar.unpacked/native/kernel_host_binding.node"
    ],
    ...overrides
  };
}

test("tryLoadNativeBindingFromResolution reports not_found when no candidate exists", () => {
  const result = tryLoadNativeBindingFromResolution(createResolution(), {
    fileExists() {
      return false;
    }
  });

  assert.deepEqual(result, {
    ok: false,
    loadState: "not_found",
    runMode: "packaged",
    bindingMechanism: "node_api_addon",
    bindingBasename: "kernel_host_binding.node",
    resolvedPath: null,
    candidates: [
      "E:/packaged/resources/native/kernel_host_binding.node",
      "E:/packaged/resources/app.asar.unpacked/native/kernel_host_binding.node"
    ],
    error: {
      code: "HOST_KERNEL_BINDING_NOT_FOUND",
      message: "Kernel native binding was not found for the current run mode."
    }
  });
});

test("tryLoadNativeBindingFromResolution reports load_failed when module loading throws", () => {
  const result = tryLoadNativeBindingFromResolution(createResolution(), {
    fileExists(candidate) {
      return candidate.endsWith("/native/kernel_host_binding.node");
    },
    loadModule() {
      throw new Error("simulated load failure");
    }
  });

  assert.equal(result.ok, false);
  assert.equal(result.loadState, "load_failed");
  assert.equal(result.resolvedPath, "E:/packaged/resources/native/kernel_host_binding.node");
  assert.deepEqual(result.error, {
    code: "HOST_KERNEL_BINDING_LOAD_FAILED",
    message: "Kernel native binding failed to load.",
    details: {
      loadMessage: "simulated load failure"
    }
  });
});

test("tryLoadNativeBindingFromResolution returns binding info when the addon loads", () => {
  const fakeBinding = {
    getBindingInfo() {
      return {
        bindingName: "kernel_host_binding"
      };
    }
  };

  const result = tryLoadNativeBindingFromResolution(createResolution({
    runMode: "dev",
    candidates: [
      "E:/repo/apps/electron/build/Debug/kernel_host_binding.node",
      "E:/repo/apps/electron/build/Release/kernel_host_binding.node"
    ]
  }), {
    fileExists(candidate) {
      return candidate.endsWith("/Debug/kernel_host_binding.node");
    },
    loadModule(candidate) {
      assert.equal(candidate, "E:/repo/apps/electron/build/Debug/kernel_host_binding.node");
      return fakeBinding;
    }
  });

  assert.equal(result.ok, true);
  assert.equal(result.loadState, "loaded");
  assert.equal(result.runMode, "dev");
  assert.equal(result.bindingMechanism, "node_api_addon");
  assert.equal(result.bindingBasename, "kernel_host_binding.node");
  assert.equal(result.resolvedPath, "E:/repo/apps/electron/build/Debug/kernel_host_binding.node");
  assert.equal(result.binding, fakeBinding);
  assert.deepEqual(result.bindingInfo, {
    bindingName: "kernel_host_binding"
  });
});
