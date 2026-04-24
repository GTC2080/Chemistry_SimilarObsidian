import { useEffect, useRef, useState } from "react";

interface SmilesViewerProps {
  smiles: string;
  width?: number;
  height?: number;
  compact?: boolean;
}

const THEME = {
  C: "#EDEDED",
  O: "#E06060",
  N: "#6090E0",
  S: "#D4A840",
  P: "#E08040",
  F: "#60D060",
  CL: "#60D060",
  BR: "#C04030",
  I: "#8040C0",
  H: "#CCCCCC",
  BACKGROUND: "transparent",
};

const PARSE_CACHE_LIMIT = 260;

let drawerModulePromise: Promise<unknown> | null = null;
const parsedTreeCache = new Map<string, unknown>();

function clearCanvas(canvas: HTMLCanvasElement) {
  const ctx = canvas.getContext("2d");
  if (!ctx) return;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
}

async function loadDrawerCtor() {
  if (!drawerModulePromise) {
    drawerModulePromise = import("smiles-drawer");
  }
  const mod = await drawerModulePromise as {
    SmiDrawer?: unknown;
    default?: { SmiDrawer?: unknown } | unknown;
  };
  const ctor = mod.SmiDrawer ?? (mod.default as { SmiDrawer?: unknown } | undefined)?.SmiDrawer ?? mod.default;
  if (!ctor) {
    throw new Error("SmiDrawer module unavailable");
  }
  return ctor as {
    new(config: Record<string, unknown>): { draw: (tree: unknown, canvas: HTMLCanvasElement, theme: string) => void };
    parse: (smiles: string, onSuccess: (tree: unknown) => void, onError: () => void) => void;
  };
}

function cacheParsedTree(smiles: string, tree: unknown) {
  if (parsedTreeCache.has(smiles)) {
    parsedTreeCache.delete(smiles);
  }
  parsedTreeCache.set(smiles, tree);
  if (parsedTreeCache.size > PARSE_CACHE_LIMIT) {
    const oldest = parsedTreeCache.keys().next().value;
    if (oldest) {
      parsedTreeCache.delete(oldest);
    }
  }
}

function parseTree(SmiDrawer: {
  parse: (smiles: string, onSuccess: (tree: unknown) => void, onError: () => void) => void;
}, smiles: string): Promise<unknown> {
  return new Promise((resolve, reject) => {
    SmiDrawer.parse(
      smiles,
      tree => resolve(tree),
      () => reject(new Error("Invalid SMILES"))
    );
  });
}

export default function SmilesViewer({ smiles, width = 320, height = 240, compact = false }: SmilesViewerProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const drawerRef = useRef<{
    key: string;
    instance: { draw: (tree: unknown, canvas: HTMLCanvasElement, theme: string) => void };
  } | null>(null);
  const lastRenderKeyRef = useRef("");
  const [isVisible, setIsVisible] = useState(true);
  const [error, setError] = useState(false);

  useEffect(() => {
    const el = containerRef.current;
    if (!el || typeof IntersectionObserver === "undefined") return;
    const observer = new IntersectionObserver(
      entries => {
        const visible = entries.some(entry => entry.isIntersecting);
        setIsVisible(prev => (prev === visible ? prev : visible));
      },
      { root: null, rootMargin: "120px", threshold: 0.01 }
    );
    observer.observe(el);
    return () => observer.disconnect();
  }, []);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const cleanSmiles = (smiles ?? "").toString().trim();
    const renderKey = `${cleanSmiles}::${width}x${height}::${compact ? "1" : "0"}`;

    if (!isVisible) {
      clearCanvas(canvas);
      return;
    }

    if (!cleanSmiles) {
      clearCanvas(canvas);
      if (error) setError(false);
      lastRenderKeyRef.current = "";
      return;
    }

    if (lastRenderKeyRef.current === renderKey && !error) {
      return;
    }

    let cancelled = false;
    (async () => {
      try {
        const SmiDrawer = await loadDrawerCtor();
        if (cancelled) return;

        const drawerKey = `${width}x${height}:${compact ? "1" : "0"}`;
        if (!drawerRef.current || drawerRef.current.key !== drawerKey) {
          drawerRef.current = {
            key: drawerKey,
            instance: new SmiDrawer({
              width,
              height,
              bondThickness: 1.2,
              bondLength: compact ? 16 : 20,
              shortBondLength: 0.8,
              fontSizeLarge: compact ? 10 : 12,
              fontSizeSmall: compact ? 7 : 8,
              padding: compact ? 14 : 24,
              themes: {
                dark: {
                  C: THEME.C,
                  O: THEME.O,
                  N: THEME.N,
                  S: THEME.S,
                  P: THEME.P,
                  F: THEME.F,
                  CL: THEME.CL,
                  BR: THEME.BR,
                  I: THEME.I,
                  H: THEME.H,
                  BACKGROUND: THEME.BACKGROUND,
                },
              },
            }),
          };
        }

        let tree = parsedTreeCache.get(cleanSmiles);
        if (!tree) {
          tree = await parseTree(SmiDrawer, cleanSmiles);
          cacheParsedTree(cleanSmiles, tree);
        }
        if (cancelled || !tree) return;

        drawerRef.current.instance.draw(tree, canvas, "dark");
        lastRenderKeyRef.current = renderKey;
        if (error) setError(false);
      } catch {
        if (cancelled) return;
        clearCanvas(canvas);
        lastRenderKeyRef.current = "";
        setError(true);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [compact, error, height, isVisible, smiles, width]);

  return (
    <div ref={containerRef} className="relative smiles-container flex items-center justify-center rounded-[12px] py-4 px-4 my-2">
      <canvas
        ref={canvasRef}
        className="smiles-canvas"
        width={width}
        height={height}
      />
      {error && (
        <div className="absolute inset-0 smiles-error flex items-center justify-center rounded-[12px]">
          <span className="smiles-error-text text-[12px] font-mono">
            [Invalid SMILES String]
          </span>
        </div>
      )}
    </div>
  );
}
