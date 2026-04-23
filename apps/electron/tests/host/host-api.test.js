const test = require("node:test");
const assert = require("node:assert/strict");

const { HostApi } = require("../../src/main/host-api");
const { HOST_ERROR_CODES } = require("../../src/shared/host-contract");

function createStubKernelAdapter(overrides = {}) {
  return {
    getBindingInfo() {
      return {
        attached: true
      };
    },
    getKernelRuntimeSummary() {
      return {
        session_state: "closed",
        index_state: "unavailable",
        indexed_note_count: 0,
        pending_recovery_ops: 0
      };
    },
    getRebuildStatusSummary() {
      return {
        in_flight: false,
        has_last_result: false,
        current_generation: 0,
        last_completed_generation: 0,
        current_started_at_ns: 0,
        last_result_code: null,
        last_result_at_ns: 0,
        index_state: "unavailable"
      };
    },
    async querySearch() {
      throw new Error("querySearch was not stubbed");
    },
    async readNote() {
      throw new Error("readNote was not stubbed");
    },
    async writeNote() {
      throw new Error("writeNote was not stubbed");
    },
    async listAttachments() {
      throw new Error("listAttachments was not stubbed");
    },
    async exportDiagnostics() {
      throw new Error("exportDiagnostics was not stubbed");
    },
    ...overrides
  };
}

function createStubFilesSurface(overrides = {}) {
  return {
    async listEntries() {
      throw new Error("listEntries was not stubbed");
    },
    async readNote() {
      throw new Error("readNote was not stubbed");
    },
    async listRecentNotes() {
      throw new Error("listRecentNotes was not stubbed");
    },
    ...overrides
  };
}

test("HostApi querySearch rejects an empty query with the frozen error envelope", async () => {
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter()
  });

  const result = await api.querySearch({
    query: "   ",
    request_id: "req-empty-query"
  });

  assert.deepEqual(result, {
    ok: false,
    data: null,
    error: {
      code: HOST_ERROR_CODES.invalidArgument,
      message: "query must be a non-empty string.",
      details: {
        field: "query"
      }
    },
    request_id: "req-empty-query"
  });
});

test("HostApi querySearch forwards the normalized request shape and maps search hits", async () => {
  let capturedRequest = null;
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter({
      async querySearch(request) {
        capturedRequest = request;
        return {
          ok: true,
          value: {
            total_hits: 1,
            has_more: false,
            hits: [
              {
                rel_path: "notes/example.md",
                title: "Example",
                snippet: "seeded body",
                match_flags: 3,
                snippet_status: 1,
                result_kind: 0,
                result_flags: 0,
                score: 2.5
              }
            ]
          }
        };
      }
    })
  });

  const result = await api.querySearch({
    query: "spectra",
    limit: 5,
    offset: 2,
    tagFilter: "chem",
    includeDeleted: true,
    sortMode: "score_desc",
    request_id: "req-search"
  });

  assert.deepEqual(capturedRequest, {
    query: "spectra",
    limit: 5,
    offset: 2,
    kind: "all",
    tagFilter: "chem",
    pathPrefix: null,
    includeDeleted: true,
    sortMode: "score_desc"
  });

  assert.deepEqual(result, {
    ok: true,
    data: {
      request: capturedRequest,
      count: 1,
      totalHits: 1,
      hasMore: false,
      items: [
        {
          relPath: "notes/example.md",
          title: "Example",
          snippet: "seeded body",
          matchFlags: 3,
          snippetStatus: 1,
          resultKind: 0,
          resultFlags: 0,
          score: 2.5
        }
      ]
    },
    error: null,
    request_id: "req-search"
  });
});

test("HostApi listAttachments maps attachment records into the host model", async () => {
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter({
      async listAttachments() {
        return {
          ok: true,
          value: {
            attachments: [
              {
                rel_path: "assets/sample.pdf",
                basename: "sample.pdf",
                extension: ".pdf",
                file_size: 42,
                mtime_ns: 99,
                ref_count: 1,
                kind: 2,
                flags: 4,
                presence: 1
              }
            ]
          }
        };
      }
    })
  });

  const result = await api.listAttachments({
    limit: 10,
    request_id: "req-attachments"
  });

  assert.deepEqual(result, {
    ok: true,
    data: {
      count: 1,
      items: [
        {
          relPath: "assets/sample.pdf",
          basename: "sample.pdf",
          extension: ".pdf",
          fileSize: 42,
          mtimeNs: 99,
          refCount: 1,
          kind: 2,
          flags: 4,
          presence: 1
        }
      ]
    },
    error: null,
    request_id: "req-attachments"
  });
});

test("HostApi exportSupportBundle preserves request id and mapped output path", async () => {
  let capturedRequest = null;
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter({
      async exportDiagnostics(request) {
        capturedRequest = request;
        return {
          ok: true,
          value: {
            result: "exported"
          }
        };
      }
    })
  });

  const result = await api.exportSupportBundle({
    outputPath: "E:/tmp/support-bundle.json",
    request_id: "req-diagnostics"
  });

  assert.deepEqual(capturedRequest, {
    outputPath: "E:/tmp/support-bundle.json"
  });
  assert.deepEqual(result, {
    ok: true,
    data: {
      outputPath: "E:/tmp/support-bundle.json",
      result: "exported"
    },
    error: null,
    request_id: "req-diagnostics"
  });
});

test("HostApi files.listEntries rejects access when no active vault session exists", async () => {
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter(),
    getActiveVaultPath: () => null,
    filesSurface: createStubFilesSurface()
  });

  const result = await api.listFilesEntries({
    request_id: "req-files-session"
  });

  assert.deepEqual(result, {
    ok: false,
    data: null,
    error: {
      code: HOST_ERROR_CODES.sessionNotOpen,
      message: "Open a vault session before using this host surface.",
      details: {
        operation: "files.list_entries",
        run_mode: "dev"
      }
    },
    request_id: "req-files-session"
  });
});

test("HostApi files.listEntries maps entry records into the host model", async () => {
  let capturedRequest = null;
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter(),
    getActiveVaultPath: () => "E:/vault",
    filesSurface: createStubFilesSurface({
      async listEntries(request) {
        capturedRequest = request;
        return {
          entries: [
            {
              rel_path: "notes/example.md",
              name: "example.md",
              title: "example",
              kind: "note",
              is_directory: false,
              size_bytes: 42,
              mtime_ms: 1000
            }
          ]
        };
      }
    })
  });

  const result = await api.listFilesEntries({
    parentRelPath: "notes",
    limit: 10,
    request_id: "req-files-list"
  });

  assert.deepEqual(capturedRequest, {
    vaultPath: "E:/vault",
    parentRelPath: "notes",
    limit: 10
  });
  assert.deepEqual(result, {
    ok: true,
    data: {
      parentRelPath: "notes",
      count: 1,
      items: [
        {
          relPath: "notes/example.md",
          name: "example.md",
          title: "example",
          kind: "note",
          isDirectory: false,
          sizeBytes: 42,
          mtimeMs: 1000
        }
      ]
    },
    error: null,
    request_id: "req-files-list"
  });
});

test("HostApi files.readNote maps a note record into the host model", async () => {
  let capturedRequest = null;
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter({
      async readNote(request) {
        capturedRequest = request;
        return {
          ok: true,
          value: {
            rel_path: "notes/example.md",
            body_text: "# Example\n\nbody",
            file_size: 16,
            mtime_ns: 2000000000,
            content_revision: "rev-example"
          }
        };
      }
    }),
    getActiveVaultPath: () => "E:/vault",
    filesSurface: createStubFilesSurface()
  });

  const result = await api.readNoteFile({
    relPath: "notes/example.md",
    request_id: "req-files-read"
  });

  assert.deepEqual(capturedRequest, {
    relPath: "notes/example.md"
  });
  assert.deepEqual(result, {
    ok: true,
    data: {
      relPath: "notes/example.md",
      name: "example.md",
      title: "Example",
      kind: "note",
      bodyText: "# Example\n\nbody",
      sizeBytes: 16,
      mtimeMs: 2000,
      contentRevision: "rev-example"
    },
    error: null,
    request_id: "req-files-read"
  });
});

test("HostApi files.writeNote forwards the optimistic revision and maps the write result", async () => {
  let capturedRequest = null;
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter({
      async writeNote(request) {
        capturedRequest = request;
        return {
          ok: true,
          value: {
            rel_path: "notes/example.md",
            body_text: "# Example\n\nbody",
            file_size: 16,
            mtime_ns: 2000000000,
            content_revision: "rev-after-write",
            disposition: "written"
          }
        };
      }
    })
  });

  const result = await api.writeNoteFile({
    relPath: "notes/example.md",
    bodyText: "# Example\n\nbody",
    expectedRevision: "rev-before-write",
    request_id: "req-files-write"
  });

  assert.deepEqual(capturedRequest, {
    relPath: "notes/example.md",
    bodyText: "# Example\n\nbody",
    expectedRevision: "rev-before-write"
  });
  assert.deepEqual(result, {
    ok: true,
    data: {
      disposition: "written",
      note: {
        relPath: "notes/example.md",
        name: "example.md",
        title: "Example",
        kind: "note",
        bodyText: "# Example\n\nbody",
        sizeBytes: 16,
        mtimeMs: 2000,
        contentRevision: "rev-after-write"
      }
    },
    error: null,
    request_id: "req-files-write"
  });
});

test("HostApi files.listRecent maps recent note records into the host model", async () => {
  const api = new HostApi({
    kernelAdapter: createStubKernelAdapter(),
    getActiveVaultPath: () => "E:/vault",
    filesSurface: createStubFilesSurface({
      async listRecentNotes() {
        return {
          entries: [
            {
              rel_path: "notes/recent.md",
              name: "recent.md",
              title: "recent",
              kind: "note",
              is_directory: false,
              size_bytes: 12,
              mtime_ms: 5000
            }
          ]
        };
      }
    })
  });

  const result = await api.listRecentFiles({
    limit: 5,
    request_id: "req-files-recent"
  });

  assert.deepEqual(result, {
    ok: true,
    data: {
      parentRelPath: null,
      count: 1,
      items: [
        {
          relPath: "notes/recent.md",
          name: "recent.md",
          title: "recent",
          kind: "note",
          isDirectory: false,
          sizeBytes: 12,
          mtimeMs: 5000
        }
      ]
    },
    error: null,
    request_id: "req-files-recent"
  });
});
