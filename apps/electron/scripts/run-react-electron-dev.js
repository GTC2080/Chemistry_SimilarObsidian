const { spawn } = require("node:child_process");
const path = require("node:path");

const cwd = path.resolve(__dirname, "..");
const env = { ...process.env };

delete env.ELECTRON_RUN_AS_NODE;

const vite = spawn("npm.cmd", ["run", "renderer:dev"], {
  cwd,
  env,
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

  const electron = spawn("node", ["./scripts/run-electron.js"], {
    cwd,
    env: childEnv,
    stdio: "inherit"
  });

  electron.on("exit", (code) => {
    vite.kill();
    process.exit(code ?? 0);
  });
}

vite.stdout.on("data", (chunk) => {
  const text = chunk.toString();
  process.stdout.write(text);
  if (text.includes("http://127.0.0.1:5173") || text.includes("http://localhost:5173")) {
    launchElectron();
  }
});

vite.stderr.on("data", (chunk) => {
  process.stderr.write(chunk.toString());
});

vite.on("error", (error) => {
  console.error(error);
  process.exit(1);
});
