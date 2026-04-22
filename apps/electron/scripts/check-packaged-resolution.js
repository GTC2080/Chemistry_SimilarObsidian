const path = require("node:path");
const { resolveBindingCandidates } = require("../src/main/kernel-adapter/native-binding-resolution");

function fail(message, details) {
  const payload = details ? ` ${JSON.stringify(details)}` : "";
  console.error(`PACKAGED_RESOLUTION_FAIL ${message}${payload}`);
  process.exit(1);
}

const appRoot = path.resolve(__dirname, "..");
const fakeResourcesPath = path.resolve(appRoot, "dist", "resources");
const resolution = resolveBindingCandidates({
  packaged: true,
  appPath: appRoot,
  resourcesPath: fakeResourcesPath
});

if (resolution.runMode !== "packaged") {
  fail("resolution did not enter packaged mode", {
    runMode: resolution.runMode
  });
}

if (!Array.isArray(resolution.candidates) || resolution.candidates.length !== 2) {
  fail("packaged mode must expose exactly two native binding candidates", {
    candidates: resolution.candidates
  });
}

if (!resolution.candidates.every((candidate) => candidate.startsWith(fakeResourcesPath))) {
  fail("packaged candidates must resolve under the packaged resources root", {
    resourcesPath: fakeResourcesPath,
    candidates: resolution.candidates
  });
}

if (resolution.candidates.some((candidate) => candidate.includes(`${path.sep}src${path.sep}`))) {
  fail("packaged candidates must not point back into source directories", {
    candidates: resolution.candidates
  });
}

console.log(`PACKAGED_RESOLUTION_OK ${JSON.stringify({
  bindingBasename: resolution.bindingBasename,
  resourcesPath: resolution.resourcesPath,
  candidates: resolution.candidates
})}`);
