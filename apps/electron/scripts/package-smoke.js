const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { spawn } = require("node:child_process");
const { assertSmokePayload } = require("./smoke-assertions");

function fail(message, details = null) {
  const suffix = details ? ` ${JSON.stringify(details)}` : "";
  console.error(`PACKAGED_SMOKE_FAIL ${message}${suffix}`);
  process.exit(1);
}

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function ensurePackager() {
  try {
    const packagerModule = require("@electron/packager");
    if (typeof packagerModule.packager !== "function") {
      fail("Loaded @electron/packager, but no packager() export was found.");
    }
    return packagerModule.packager;
  } catch (error) {
    fail("Missing @electron/packager. Run npm install first.", {
      loadMessage: error instanceof Error ? error.message : String(error)
    });
  }
}

async function waitForSmokeResult(resultPath, progressPath, timeoutMs) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    if (fs.existsSync(resultPath)) {
      return JSON.parse(fs.readFileSync(resultPath, "utf8"));
    }

    await wait(250);
  }

  fail("Timed out waiting for packaged smoke result.", {
    resultPath,
    progressPath,
    progress: fs.existsSync(progressPath)
      ? fs.readFileSync(progressPath, "utf8")
      : null
  });
}

async function main() {
  const packager = await ensurePackager();
  const appRoot = path.resolve(__dirname, "..");
  const packageOutRoot = path.join(appRoot, "out", "packaged-smoke");
  const nativeSourcePath = path.join(appRoot, "build", "Debug", "kernel_host_binding.node");
  const runId = `pkg-${Date.now()}`;
  const smokeRoot = path.join(process.env.APPDATA || path.join(os.homedir(), "AppData", "Roaming"), "chemistry-obsidian-electron-host", "smoke");
  const resultPath = path.join(smokeRoot, `result-${runId}.json`);
  const progressPath = path.join(smokeRoot, `progress-${runId}.log`);

  if (!fs.existsSync(nativeSourcePath)) {
    fail("Native binding is missing. Run npm run build:native first.", {
      nativeSourcePath
    });
  }

  fs.rmSync(packageOutRoot, { recursive: true, force: true });
  fs.rmSync(resultPath, { force: true });
  fs.rmSync(progressPath, { force: true });
  fs.mkdirSync(packageOutRoot, { recursive: true });

  const packagedAppPaths = await packager({
    dir: appRoot,
    out: packageOutRoot,
    overwrite: true,
    platform: "win32",
    arch: "x64",
    asar: false,
    prune: true,
    name: "ChemistryObsidianHostSmoke",
    ignore: [
      /^\/out($|\/)/,
      /^\/dist($|\/)/,
      /^\/build\/native($|\/)/,
      /^\/tests($|\/)/
    ]
  });

  if (!Array.isArray(packagedAppPaths) || packagedAppPaths.length !== 1) {
    fail("Electron packager returned an unexpected output set.", {
      packagedAppPaths
    });
  }

  const packagedAppRoot = packagedAppPaths[0];
  const resourcesRoot = path.join(packagedAppRoot, "resources");
  const nativeTargetDir = path.join(resourcesRoot, "native");
  const executablePath = path.join(packagedAppRoot, "ChemistryObsidianHostSmoke.exe");

  fs.mkdirSync(nativeTargetDir, { recursive: true });
  fs.copyFileSync(nativeSourcePath, path.join(nativeTargetDir, "kernel_host_binding.node"));

  if (!fs.existsSync(executablePath)) {
    fail("Packaged Electron executable was not created.", {
      executablePath
    });
  }

  const childEnv = {
    ...process.env,
    CHEM_OBSIDIAN_HOST_SMOKE: "1",
    CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID: runId
  };
  delete childEnv.ELECTRON_RUN_AS_NODE;

  const child = spawn(executablePath, [], {
    cwd: packagedAppRoot,
    env: childEnv,
    stdio: "ignore",
    windowsHide: true
  });

  await new Promise((resolve, reject) => {
    child.once("spawn", resolve);
    child.once("error", reject);
  });

  const smokeResult = await waitForSmokeResult(resultPath, progressPath, 60000);

  if (!smokeResult || smokeResult.ok !== true) {
    fail("Packaged smoke returned a failure payload.", smokeResult);
  }

  assertSmokePayload(smokeResult.payload);
  console.log(`PACKAGED_SMOKE_OK ${JSON.stringify({
    packagedAppRoot,
    nativeBindingPath: path.join(nativeTargetDir, "kernel_host_binding.node"),
    resultPath,
    progressPath
  })}`);
}

main().catch((error) => {
  fail(error instanceof Error ? error.stack || error.message : String(error));
});
