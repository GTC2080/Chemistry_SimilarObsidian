const { spawn } = require("node:child_process");

const electronBinary = require("electron");
const env = { ...process.env };

delete env.ELECTRON_RUN_AS_NODE;

const child = spawn(electronBinary, ["."], {
  env,
  stdio: "inherit"
});

child.on("error", (error) => {
  console.error(`ELECTRON_LAUNCH_FAIL ${error && error.stack ? error.stack : error}`);
  process.exit(1);
});

child.on("exit", (code, signal) => {
  if (signal) {
    console.error(`ELECTRON_LAUNCH_SIGNAL ${signal}`);
    process.exit(1);
  }

  process.exit(code ?? 0);
});
