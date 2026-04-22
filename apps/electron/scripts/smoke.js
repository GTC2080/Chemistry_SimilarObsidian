const { spawn } = require("node:child_process");
const { assertSmokePayload } = require("./smoke-assertions");

const env = {
  ...process.env,
  ELECTRON_SMOKE: "1"
};

delete env.ELECTRON_RUN_AS_NODE;

const child = spawn(process.execPath, ["./scripts/run-electron.js"], {
  cwd: __dirname + "/..",
  env,
  stdio: ["ignore", "pipe", "pipe"]
});

let stdout = "";
let stderr = "";

child.stdout.on("data", (chunk) => {
  const text = chunk.toString();
  stdout += text;
  process.stdout.write(text);
});

child.stderr.on("data", (chunk) => {
  const text = chunk.toString();
  stderr += text;
  process.stderr.write(text);
});

child.on("exit", (code) => {
  if (code !== 0 || !stdout.includes("ELECTRON_SMOKE_OK")) {
    if (!stderr.trim()) {
      console.error("ELECTRON_SMOKE_FAIL Smoke marker was not observed.");
    }
    process.exit(code ?? 1);
  }

  try {
    const marker = "ELECTRON_SMOKE_OK ";
    const markerIndex = stdout.lastIndexOf(marker);
    if (markerIndex < 0) {
      throw new Error("Smoke marker payload was not found.");
    }

    const payloadText = stdout.slice(markerIndex + marker.length).trim();
    const payload = JSON.parse(payloadText);
    assertSmokePayload(payload);
  } catch (error) {
    console.error(`ELECTRON_SMOKE_FAIL ${error && error.stack ? error.stack : error}`);
    process.exit(1);
    return;
  }

  process.exit(0);
});
