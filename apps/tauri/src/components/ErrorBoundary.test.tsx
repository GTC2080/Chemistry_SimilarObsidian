import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import ErrorBoundary from "./ErrorBoundary";
import { LanguageProvider } from "../i18n";

// A component that throws on demand
function ThrowingChild({ shouldThrow }: { shouldThrow: boolean }) {
  if (shouldThrow) {
    throw new Error("test explosion");
  }
  return <div>child content</div>;
}

function wrap(ui: React.ReactNode) {
  return <LanguageProvider language="zh-CN">{ui}</LanguageProvider>;
}

describe("ErrorBoundary", () => {
  // Suppress noisy error boundary console.error logs during tests
  const originalConsoleError = console.error;
  beforeEach(() => {
    console.error = vi.fn();
  });
  afterEach(() => {
    console.error = originalConsoleError;
  });

  it("renders children when no error occurs", () => {
    render(
      wrap(
        <ErrorBoundary>
          <ThrowingChild shouldThrow={false} />
        </ErrorBoundary>,
      ),
    );
    expect(screen.getByText("child content")).toBeTruthy();
  });

  it("shows default error UI when a child throws", () => {
    render(
      wrap(
        <ErrorBoundary>
          <ThrowingChild shouldThrow={true} />
        </ErrorBoundary>,
      ),
    );

    expect(screen.getByText("组件渲染出错")).toBeTruthy();
    expect(screen.getByText("test explosion")).toBeTruthy();
    expect(screen.getByText("重新加载")).toBeTruthy();
  });

  it("renders custom fallback when provided", () => {
    render(
      wrap(
        <ErrorBoundary fallback={<div>custom fallback</div>}>
          <ThrowingChild shouldThrow={true} />
        </ErrorBoundary>,
      ),
    );

    expect(screen.getByText("custom fallback")).toBeTruthy();
    expect(screen.queryByText("组件渲染出错")).toBeNull();
  });

  it("calls onError callback when a child throws", () => {
    const onError = vi.fn();

    render(
      wrap(
        <ErrorBoundary onError={onError}>
          <ThrowingChild shouldThrow={true} />
        </ErrorBoundary>,
      ),
    );

    expect(onError).toHaveBeenCalledTimes(1);
    expect(onError.mock.calls[0][0]).toBeInstanceOf(Error);
    expect(onError.mock.calls[0][0].message).toBe("test explosion");
  });

  it("resets error state when retry button is clicked", () => {
    let shouldThrow = true;

    function ConditionalChild() {
      if (shouldThrow) throw new Error("boom");
      return <div>recovered</div>;
    }

    render(
      wrap(
        <ErrorBoundary>
          <ConditionalChild />
        </ErrorBoundary>,
      ),
    );

    expect(screen.getByText("组件渲染出错")).toBeTruthy();

    // Now flip the flag so re-render succeeds
    shouldThrow = false;
    fireEvent.click(screen.getByText("重新加载"));

    expect(screen.getByText("recovered")).toBeTruthy();
    expect(screen.queryByText("组件渲染出错")).toBeNull();
  });
});
