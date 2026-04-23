const { spawn } = require("node:child_process");
const path = require("node:path");

const cwd = path.resolve(__dirname, "..");
const env = { ...process.env };
const useShell = process.platform === "win32";

delete env.ELECTRON_RUN_AS_NODE;

const vite = spawn("npm", ["run", "renderer:dev"], {
  cwd,
  env,
  shell: useShell,
  stdio: ["ignore", "pipe", "pipe"]
});

let launchedElectron = false;

function launchElectron() {
  if (launchedElectron) {
    return;
  }

  launchedElectron = true;
  const childEnv = {
    ...env,
    CHEM_OBSIDIAN_RENDERER_URL: "http://127.0.0.1:5173"
  };

  const electron = spawn(process.execPath, ["./scripts/run-electron.js"], {
    cwd,
    env: childEnv,
    stdio: "inherit"
  });

  electron.on("exit", (code) => {
    vite.kill();
    process.exit(code ?? 0);
  });
}

function handleViteOutput(chunk) {
  const text = chunk.toString();
  process.stdout.write(text);
  if (text.includes("http://127.0.0.1:5173") || text.includes("http://localhost:5173")) {
    launchElectron();
  }
}

vite.stdout.on("data", handleViteOutput);

vite.stderr.on("data", (chunk) => {
  handleViteOutput(chunk);
});

setTimeout(launchElectron, 2500);

vite.on("error", (error) => {
  console.error(error);
  process.exit(1);
});
