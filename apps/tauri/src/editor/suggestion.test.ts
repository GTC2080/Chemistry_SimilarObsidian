import { describe, expect, it, vi, beforeEach, afterEach } from "vitest";
import { createDebouncedWikiLinkItems } from "./suggestion";
import type { NoteInfo } from "../types";

function note(id: string): NoteInfo {
  return {
    id,
    name: id,
    path: `C:/vault/${id}`,
    created_at: 1,
    updated_at: 1,
    file_extension: "md",
  };
}

describe("WikiLink suggestion search", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("coalesces rapid query updates and searches only the latest query", async () => {
    const search = vi.fn(async (query: string) => [note(query)]);
    const items = createDebouncedWikiLinkItems("C:/vault", search, 100);

    const first = items({ query: "a" });
    vi.advanceTimersByTime(50);
    const second = items({ query: "alpha" });

    await vi.advanceTimersByTimeAsync(100);

    await expect(first).resolves.toEqual([]);
    await expect(second).resolves.toEqual([note("alpha")]);
    expect(search).toHaveBeenCalledTimes(1);
    expect(search).toHaveBeenCalledWith("alpha");
  });

  it("drops stale in-flight search results when a newer query starts", async () => {
    let resolveFirst: (value: NoteInfo[]) => void = () => {};
    const search = vi
      .fn()
      .mockImplementationOnce(
        () => new Promise<NoteInfo[]>((resolve) => { resolveFirst = resolve; }),
      )
      .mockImplementationOnce(async (query: string) => [note(query)]);
    const items = createDebouncedWikiLinkItems("C:/vault", search, 1);

    const first = items({ query: "a" });
    await vi.advanceTimersByTimeAsync(1);
    const second = items({ query: "alpha" });
    await vi.advanceTimersByTimeAsync(1);

    resolveFirst([note("a")]);

    await expect(second).resolves.toEqual([note("alpha")]);
    await expect(first).resolves.toEqual([]);
    expect(search).toHaveBeenCalledTimes(2);
  });

  it("drops an in-flight search result after the query is cleared", async () => {
    let resolveFirst: (value: NoteInfo[]) => void = () => {};
    const search = vi.fn(
      () => new Promise<NoteInfo[]>((resolve) => { resolveFirst = resolve; }),
    );
    const items = createDebouncedWikiLinkItems("C:/vault", search, 1);

    const first = items({ query: "alpha" });
    await vi.advanceTimersByTimeAsync(1);
    const cleared = items({ query: "" });

    resolveFirst([note("alpha")]);

    await expect(cleared).resolves.toEqual([]);
    await expect(first).resolves.toEqual([]);
    expect(search).toHaveBeenCalledTimes(1);
  });
});
