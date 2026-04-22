const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");

function fail(message) {
  console.error(`NATIVE_BUILD_FAIL ${message}`);
  process.exit(1);
}

function findNodeHeadersDir() {
  const cacheRoot = path.join(process.env.LOCALAPPDATA || "", "node-gyp", "Cache");
  if (!cacheRoot || !fs.existsSync(cacheRoot)) {
    return null;
  }

  const entries = fs.readdirSync(cacheRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .map((entry) => entry.name)
    .sort((left, right) => right.localeCompare(left, undefined, { numeric: true }));

  for (const entry of entries) {
    const candidate = path.join(cacheRoot, entry);
    const headerPath = path.join(candidate, "include", "node", "node_api.h");
    if (fs.existsSync(headerPath)) {
      return candidate;
    }
  }

  return null;
}

function runCmake(args, workdir) {
  const result = spawnSync("cmake", args, {
    cwd: workdir,
    stdio: "inherit",
    shell: false
  });

  if (result.status !== 0) {
    fail(`cmake ${args.join(" ")} exited with status ${result.status ?? "unknown"}.`);
  }
}

async function ensureElectronNodeLib(appRoot) {
  const electronVersion = require(path.join(appRoot, "node_modules", "electron", "package.json")).version;
  const targetDir = path.join(appRoot, ".electron-headers", `v${electronVersion}`, "win-x64");
  const targetPath = path.join(targetDir, "node.lib");

  if (fs.existsSync(targetPath)) {
    return targetPath;
  }

  fs.mkdirSync(targetDir, { recursive: true });

  const url = `https://electronjs.org/headers/v${electronVersion}/win-x64/node.lib`;
  const response = await fetch(url);
  if (!response.ok) {
    fail(`failed to download Electron node.lib from ${url} (${response.status} ${response.statusText})`);
  }

  const buffer = Buffer.from(await response.arrayBuffer());
  fs.writeFileSync(targetPath, buffer);
  return targetPath;
}

async function main() {
  const appRoot = path.resolve(__dirname, "..");
  const nativeSourceDir = path.join(appRoot, "native");
  const nativeBuildDir = path.join(appRoot, "build", "native");
  const kernelBuildDir = path.resolve(appRoot, "..", "..", "kernel", "out", "build");
  const nodeAddonApiDir = path.join(appRoot, "node_modules", "node-addon-api");
  const nodeHeadersDir = findNodeHeadersDir();
  const electronNodeLib = await ensureElectronNodeLib(appRoot);

  if (!fs.existsSync(path.join(kernelBuildDir, "src", "Debug", "chem_kernel.lib"))) {
    fail(`sealed kernel library was not found at ${path.join(kernelBuildDir, "src", "Debug", "chem_kernel.lib")}`);
  }

  if (!fs.existsSync(path.join(nodeAddonApiDir, "napi.h"))) {
    fail(`node-addon-api headers were not found at ${nodeAddonApiDir}`);
  }

  if (!nodeHeadersDir) {
    fail("no usable Node headers cache was found under %LOCALAPPDATA%\\node-gyp\\Cache");
  }

  fs.mkdirSync(nativeBuildDir, { recursive: true });

  runCmake(
    [
      "-S",
      nativeSourceDir,
      "-B",
      nativeBuildDir,
      `-DKERNEL_BUILD_DIR=${kernelBuildDir}`,
      `-DNODE_HEADERS_DIR=${nodeHeadersDir}`,
      `-DNODE_ADDON_API_DIR=${nodeAddonApiDir}`,
      `-DELECTRON_NODE_LIB=${electronNodeLib}`
    ],
    appRoot
  );

  runCmake(
    [
      "--build",
      nativeBuildDir,
      "--config",
      "Debug"
    ],
    appRoot
  );

  console.log(`NATIVE_BUILD_OK ${path.join(appRoot, "build", "Debug", "kernel_host_binding.node")}`);
}

main().catch((error) => {
  fail(error instanceof Error ? error.stack || error.message : String(error));
});
