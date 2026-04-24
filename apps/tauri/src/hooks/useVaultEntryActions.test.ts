import { describe, it, expect, vi, beforeEach } from "vitest";
import { renderHook, act } from "@testing-library/react";
import { useVaultEntryActions } from "./useVaultEntryActions";

// Mock @tauri-apps/api/core
vi.mock("@tauri-apps/api/core", () => ({
  invoke: vi.fn(),
}));

import { invoke } from "@tauri-apps/api/core";
const mockInvoke = vi.mocked(invoke);

function createMockParams(overrides: Record<string, unknown> = {}) {
  return {
    vaultPath: "/test/vault",
    ignoredFolders: "",
    activeNote: null,
    setNotes: vi.fn(),
    setActiveNote: vi.fn(),
    setNoteContent: vi.fn(),
    setLiveContent: vi.fn(),
    setError: vi.fn(),
    onSelectNote: vi.fn().mockResolvedValue(undefined),
    ...overrides,
  };
}

describe("useVaultEntryActions", () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Default: scan_vault returns empty array
    mockInvoke.mockResolvedValue([]);
  });

  it("returns all expected action functions", () => {
    const params = createMockParams();
    const { result } = renderHook(() => useVaultEntryActions(params));

    expect(typeof result.current.handleCreateFile).toBe("function");
    expect(typeof result.current.handleDeleteEntry).toBe("function");
    expect(typeof result.current.handleMoveEntry).toBe("function");
    expect(typeof result.current.handleCreateFolder).toBe("function");
    expect(typeof result.current.handleRenameEntryInline).toBe("function");
    expect(typeof result.current.handleRenameEntry).toBe("function");
  });

  describe("handleCreateFile", () => {
    it("calls write_note and scan_vault for a new markdown note", async () => {
      const params = createMockParams();
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleCreateFile("note");
      });

      // First call: write_note, second call: scan_vault (from refreshNotes)
      expect(mockInvoke).toHaveBeenCalledWith(
        "write_note",
        expect.objectContaining({
          vaultPath: "/test/vault",
          content: "# 未命名\n",
        }),
      );
      expect(mockInvoke).toHaveBeenCalledWith(
        "scan_vault",
        expect.objectContaining({
          vaultPath: "/test/vault",
          ignoredFolders: "",
        }),
      );
    });

    it("calls write_note with empty content for mol", async () => {
      const params = createMockParams();
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleCreateFile("mol");
      });

      const writeCall = mockInvoke.mock.calls.find(
        (call) => call[0] === "write_note",
      );
      expect(writeCall).toBeTruthy();
      const content = (writeCall![1] as Record<string, string>).content;
      expect(content).toBe("");
    });

    it("does nothing when vaultPath is empty", async () => {
      const params = createMockParams({ vaultPath: "" });
      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleCreateFile("note");
      });

      expect(mockInvoke).not.toHaveBeenCalled();
    });

    it("sets error on failure", async () => {
      const setError = vi.fn();
      const params = createMockParams({ setError });
      mockInvoke.mockRejectedValueOnce(new Error("disk full"));

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleCreateFile("note");
      });

      expect(setError).toHaveBeenCalledWith(
        expect.stringContaining("新建失败"),
      );
    });
  });

  describe("handleDeleteEntry", () => {
    it("calls delete_entry after user confirms", async () => {
      vi.spyOn(window, "confirm").mockReturnValue(true);
      const params = createMockParams();
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleDeleteEntry(
          "/test/vault/note.md",
          "note.md",
          false,
        );
      });

      expect(mockInvoke).toHaveBeenCalledWith(
        "delete_entry",
        expect.objectContaining({
          vaultPath: "/test/vault",
          targetPath: "/test/vault/note.md",
        }),
      );
    });

    it("does nothing when user cancels confirmation", async () => {
      vi.spyOn(window, "confirm").mockReturnValue(false);
      const params = createMockParams();

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleDeleteEntry(
          "/test/vault/note.md",
          "note.md",
          false,
        );
      });

      expect(mockInvoke).not.toHaveBeenCalled();
    });

    it("clears active note when deleting the currently active file", async () => {
      vi.spyOn(window, "confirm").mockReturnValue(true);
      const setActiveNote = vi.fn();
      const setNoteContent = vi.fn();
      const setLiveContent = vi.fn();
      const params = createMockParams({
        activeNote: {
          id: "note.md",
          name: "note",
          path: "/test/vault/note.md",
          created_at: 0,
          updated_at: 0,
          file_extension: "md",
        },
        setActiveNote,
        setNoteContent,
        setLiveContent,
      });
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleDeleteEntry(
          "/test/vault/note.md",
          "note.md",
          false,
        );
      });

      expect(setActiveNote).toHaveBeenCalledWith(null);
      expect(setNoteContent).toHaveBeenCalledWith("");
      expect(setLiveContent).toHaveBeenCalledWith("");
    });
  });

  describe("handleMoveEntry", () => {
    it("calls move_entry with correct source and dest paths", async () => {
      const params = createMockParams();
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleMoveEntry("notes/file.md", "archive");
      });

      expect(mockInvoke).toHaveBeenCalledWith(
        "move_entry",
        expect.objectContaining({
          vaultPath: "/test/vault",
          sourcePath: "/test/vault/notes/file.md",
          destFolder: "/test/vault/archive",
        }),
      );
    });

    it("uses vault root when dest folder is empty string", async () => {
      const params = createMockParams();
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleMoveEntry("subfolder/file.md", "");
      });

      const moveCall = mockInvoke.mock.calls.find(
        (c) => c[0] === "move_entry",
      );
      expect(
        (moveCall![1] as Record<string, string>).destFolder,
      ).toBe("/test/vault");
    });
  });

  describe("handleRenameEntryInline", () => {
    it("calls rename_entry with trimmed name", async () => {
      const params = createMockParams();
      mockInvoke.mockResolvedValue([]);

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleRenameEntryInline(
          "old-name.md",
          "new-name.md",
        );
      });

      expect(mockInvoke).toHaveBeenCalledWith(
        "rename_entry",
        expect.objectContaining({
          vaultPath: "/test/vault",
          sourcePath: "/test/vault/old-name.md",
          newName: "new-name.md",
        }),
      );
    });

    it("rejects names containing slashes", async () => {
      const setError = vi.fn();
      const params = createMockParams({ setError });

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleRenameEntryInline(
          "file.md",
          "bad/name.md",
        );
      });

      expect(setError).toHaveBeenCalledWith(
        expect.stringContaining("不能包含"),
      );
      expect(mockInvoke).not.toHaveBeenCalled();
    });

    it("does nothing for empty name", async () => {
      const params = createMockParams();

      const { result } = renderHook(() => useVaultEntryActions(params));

      await act(async () => {
        await result.current.handleRenameEntryInline("file.md", "  ");
      });

      expect(mockInvoke).not.toHaveBeenCalled();
    });
  });
});
