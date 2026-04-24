import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { renderHook, act } from "@testing-library/react";
import { useDebounce } from "./useDebounce";

describe("useDebounce", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("delays execution by the specified amount", () => {
    const callback = vi.fn();
    const { result } = renderHook(() => useDebounce(callback, 300));

    act(() => {
      result.current("hello");
    });

    // Not called yet
    expect(callback).not.toHaveBeenCalled();

    // Advance partially
    act(() => {
      vi.advanceTimersByTime(200);
    });
    expect(callback).not.toHaveBeenCalled();

    // Advance to the full delay
    act(() => {
      vi.advanceTimersByTime(100);
    });
    expect(callback).toHaveBeenCalledTimes(1);
    expect(callback).toHaveBeenCalledWith("hello");
  });

  it("only fires once for rapid consecutive calls (last value wins)", () => {
    const callback = vi.fn();
    const { result } = renderHook(() => useDebounce(callback, 200));

    act(() => {
      result.current("a");
    });

    act(() => {
      vi.advanceTimersByTime(50);
    });

    act(() => {
      result.current("b");
    });

    act(() => {
      vi.advanceTimersByTime(50);
    });

    act(() => {
      result.current("c");
    });

    // Now wait for the full delay after the last call
    act(() => {
      vi.advanceTimersByTime(200);
    });

    expect(callback).toHaveBeenCalledTimes(1);
    expect(callback).toHaveBeenCalledWith("c");
  });

  it("cleans up timer on unmount", () => {
    const callback = vi.fn();
    const { result, unmount } = renderHook(() => useDebounce(callback, 300));

    act(() => {
      result.current("value");
    });

    unmount();

    act(() => {
      vi.advanceTimersByTime(500);
    });

    expect(callback).not.toHaveBeenCalled();
  });

  it("uses the latest function reference", () => {
    const first = vi.fn();
    const second = vi.fn();
    let fn = first;

    const { result, rerender } = renderHook(() => useDebounce(fn, 100));

    act(() => {
      result.current();
    });

    // Switch the function before the timer fires
    fn = second;
    rerender();

    act(() => {
      vi.advanceTimersByTime(100);
    });

    expect(first).not.toHaveBeenCalled();
    expect(second).toHaveBeenCalledTimes(1);
  });
});
