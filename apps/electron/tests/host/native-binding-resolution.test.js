const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");

const {
  BINDING_BASENAME,
  resolveBindingCandidates
} = require("../../src/main/kernel-adapter/native-binding-resolution");

test("resolveBindingCandidates returns dev-mode candidates under the app build tree", () => {
  const appRoot = path.resolve("E:/repo/apps/electron");
  const resolution = resolveBindingCandidates({
    packaged: false,
    appPath: appRoot
  });

  assert.equal(resolution.runMode, "dev");
  assert.equal(resolution.bindingBasename, BINDING_BASENAME);
  assert.deepEqual(resolution.candidates, [
    path.resolve(appRoot, "build", "Debug", BINDING_BASENAME),
    path.resolve(appRoot, "build", "Release", BINDING_BASENAME),
    path.resolve(appRoot, "build", "default", BINDING_BASENAME)
  ]);
});

test("resolveBindingCandidates returns packaged-mode candidates under resourcesPath only", () => {
  const appRoot = path.resolve("E:/repo/apps/electron");
  const resourcesRoot = path.resolve("E:/repo/apps/electron/dist/resources");
  const resolution = resolveBindingCandidates({
    packaged: true,
    appPath: appRoot,
    resourcesPath: resourcesRoot
  });

  assert.equal(resolution.runMode, "packaged");
  assert.deepEqual(resolution.candidates, [
    path.resolve(resourcesRoot, "native", BINDING_BASENAME),
    path.resolve(resourcesRoot, "app.asar.unpacked", "native", BINDING_BASENAME)
  ]);
  assert.ok(resolution.candidates.every((candidate) => candidate.startsWith(resourcesRoot)));
});
