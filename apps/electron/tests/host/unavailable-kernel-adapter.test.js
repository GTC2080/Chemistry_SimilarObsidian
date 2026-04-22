const test = require("node:test");
const assert = require("node:assert/strict");

const {
  createUnavailableKernelAdapter
} = require("../../src/main/kernel-adapter/unavailable-kernel-adapter");
const { HOST_ERROR_CODES } = require("../../src/shared/host-contract");

test("unavailable kernel adapter exposes detached summaries and initialization-level failure info", async () => {
  const adapter = createUnavailableKernelAdapter({
    adapterState: "load_failed",
    failureCode: HOST_ERROR_CODES.kernelBindingLoadFailed,
    failureMessage: "Kernel native binding failed to load.",
    bindingMechanism: "node_api_addon",
    runMode: "packaged",
    bindingPath: "E:/app/resources/native/kernel_host_binding.node",
    bindingCandidates: [
      "E:/app/resources/native/kernel_host_binding.node",
      "E:/app/resources/app.asar.unpacked/native/kernel_host_binding.node"
    ],
    failureDetails: {
      loadMessage: "missing dependency"
    }
  });

  assert.deepEqual(adapter.getBindingInfo(), {
    adapter_state: "load_failed",
    attached: false,
    native_binary_mode: "packaged",
    native_binary_path: "E:/app/resources/native/kernel_host_binding.node",
    native_binary_loaded: false,
    binding_mechanism: "node_api_addon",
    binding_candidates: [
      "E:/app/resources/native/kernel_host_binding.node",
      "E:/app/resources/app.asar.unpacked/native/kernel_host_binding.node"
    ],
    failure_code: HOST_ERROR_CODES.kernelBindingLoadFailed
  });

  assert.deepEqual(adapter.getKernelRuntimeSummary(), {
    session_state: "closed",
    index_state: "unavailable",
    indexed_note_count: 0,
    pending_recovery_ops: 0
  });

  const searchResult = await adapter.querySearch();
  assert.deepEqual(searchResult, {
    ok: false,
    value: null,
    error: {
      code: HOST_ERROR_CODES.kernelBindingLoadFailed,
      message: "Kernel native binding failed to load.",
      details: {
        adapter_state: "load_failed",
        operation: "search.query",
        binding_mechanism: "node_api_addon",
        run_mode: "packaged",
        binding_path: "E:/app/resources/native/kernel_host_binding.node",
        binding_candidates: [
          "E:/app/resources/native/kernel_host_binding.node",
          "E:/app/resources/app.asar.unpacked/native/kernel_host_binding.node"
        ],
        loadMessage: "missing dependency"
      }
    }
  });
});
