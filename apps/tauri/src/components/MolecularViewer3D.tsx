import { useRef, useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { MolecularPreviewMeta } from "../types";
import { useT } from "../i18n";

interface MolecularViewer3DProps {
  data: string;
  format: string; // "pdb" | "xyz" | "cif"
  filePath: string;
  previewMeta: MolecularPreviewMeta | null;
}

export default function MolecularViewer3D({ data, format, filePath, previewMeta }: MolecularViewer3DProps) {
  const t = useT();
  const containerRef = useRef<HTMLDivElement>(null);
  const viewerRef = useRef<any>(null);
  const [modelData, setModelData] = useState(data);
  const [usingFullPrecision, setUsingFullPrecision] = useState(!previewMeta?.truncated);
  const [loadingFullPrecision, setLoadingFullPrecision] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    setModelData(data);
    setUsingFullPrecision(!previewMeta?.truncated);
    setLoadingFullPrecision(false);
  }, [data, previewMeta?.truncated, filePath]);

  const canUpgradePrecision = !!previewMeta?.truncated && !usingFullPrecision;

  const handleLoadFullPrecision = async () => {
    setLoadingFullPrecision(true);
    setError(null);
    try {
      const fullData = await invoke<string>("read_note", { filePath });
      setModelData(fullData);
      setUsingFullPrecision(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setLoadingFullPrecision(false);
    }
  };

  useEffect(() => {
    setLoading(true);
    setError(null);
    if (!containerRef.current) return;
    if (!modelData.trim()) {
      setLoading(false);
      setError(t("molecule.empty"));
      return;
    }

    let destroyed = false;
    let cleanupResize: (() => void) | null = null;

    (async () => {
      try {
        // Dynamic import to avoid loading WebGL on non-chemistry pages
        const $3Dmol = await import("3dmol");

        if (destroyed || !containerRef.current) return;

        // Clear any prior viewer
        if (viewerRef.current) {
          try { viewerRef.current.clear(); } catch { /* ignore */ }
          viewerRef.current = null;
        }
        containerRef.current.innerHTML = "";

        const viewer = $3Dmol.createViewer(containerRef.current, {
          backgroundColor: "rgba(0,0,0,0)",
          antialias: true,
          cartoonQuality: 8,
        });

        viewerRef.current = viewer;

        // Map format string for 3Dmol
        const formatMap: Record<string, string> = {
          pdb: "pdb",
          xyz: "xyz",
          cif: "cif",
        };
        const mol3dFormat = formatMap[format.toLowerCase()] ?? "pdb";

        viewer.addModel(modelData, mol3dFormat);

        // Default style: ball+stick for small molecules, cartoon+stick for proteins
        const atomCount = viewer.getModel(0)?.selectedAtoms({})?.length ?? 0;
        if (atomCount > 500) {
          // Large structures (proteins) — cartoon + muted stick for ligands
          viewer.setStyle({}, { cartoon: { color: "spectrum", opacity: 0.9 } });
          viewer.setStyle({ hetflag: true }, { stick: { radius: 0.15, colorscheme: "Jmol" } });
        } else {
          // Small molecules — ball+stick
          viewer.setStyle({}, {
            stick: { radius: 0.14, colorscheme: "Jmol" },
            sphere: { scale: 0.25, colorscheme: "Jmol" },
          });
        }

        viewer.zoomTo();
        viewer.render();
        viewer.zoom(0.9, 300);

        const handleResize = () => {
          if (!viewerRef.current) return;
          try {
            viewerRef.current.resize();
            viewerRef.current.render();
          } catch {
            // ignore resize issues during teardown
          }
        };

        if (typeof ResizeObserver !== "undefined" && containerRef.current) {
          const observer = new ResizeObserver(() => handleResize());
          observer.observe(containerRef.current);
          cleanupResize = () => observer.disconnect();
        } else {
          window.addEventListener("resize", handleResize);
          cleanupResize = () => window.removeEventListener("resize", handleResize);
        }
        requestAnimationFrame(handleResize);

        if (!destroyed) {
          setLoading(false);
          setError(null);
        }
      } catch (e) {
        if (!destroyed) {
          setError(e instanceof Error ? e.message : String(e));
          setLoading(false);
        }
      }
    })();

    return () => {
      destroyed = true;
      cleanupResize?.();
      // Critical: destroy WebGL context to prevent memory leaks
      if (viewerRef.current) {
        try {
          viewerRef.current.clear();
          viewerRef.current = null;
        } catch { /* ignore cleanup errors */ }
      }
      if (containerRef.current) {
        containerRef.current.innerHTML = "";
      }
    };
  }, [modelData, format]);

  if (error) {
    return (
      <div className="flex-1 flex items-center justify-center">
        <div className="text-center px-8 max-w-md">
          <svg className="mx-auto w-10 h-10 mb-4 text-[var(--text-quaternary)]"
            viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5"
            strokeLinecap="round" strokeLinejoin="round">
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
          <p className="text-sm text-[var(--text-tertiary)] mb-2">{t("molecule.renderFailed")}</p>
          <p className="text-xs text-[var(--text-quaternary)] font-mono break-all">{error}</p>
        </div>
      </div>
    );
  }

  return (
    <div className="flex-1 relative" style={{ background: "#0A0A0A" }}>
      <div className="absolute top-3 right-3 z-20 flex items-center gap-2">
        {previewMeta && (
          <span className="px-2.5 py-1 rounded-md text-[11px] bg-[rgba(0,0,0,0.45)] border border-[rgba(255,255,255,0.12)] text-[var(--text-quaternary)]">
            {previewMeta.truncated && !usingFullPrecision
              ? `${t("molecule.preview")} ${previewMeta.preview_atom_count}/${previewMeta.atom_count} atoms`
              : (previewMeta.atom_count > 0 ? `${t("molecule.fullPrecision")} ${previewMeta.atom_count} atoms` : t("molecule.fullPrecision"))}
          </span>
        )}
        {canUpgradePrecision && (
          <button
            type="button"
            onClick={() => { void handleLoadFullPrecision(); }}
            disabled={loadingFullPrecision}
            className="px-3 py-1 rounded-md text-[11px] font-medium cursor-pointer transition-colors
              bg-[rgba(10,132,255,0.2)] border border-[rgba(10,132,255,0.45)] text-[#cce4ff] hover:bg-[rgba(10,132,255,0.3)]
              disabled:opacity-60 disabled:cursor-not-allowed"
          >
            {loadingFullPrecision ? t("molecule.loading") : t("molecule.loadFull")}
          </button>
        )}
      </div>
      {loading && (
        <div className="absolute inset-0 flex items-center justify-center z-10">
          <div className="flex items-center gap-3">
            <div className="w-5 h-5 border-2 border-[var(--accent)] border-t-transparent rounded-full animate-spin" />
            <span className="text-sm text-[var(--text-quaternary)]">{t("molecule.loadingStructure")}</span>
          </div>
        </div>
      )}
      <div
        ref={containerRef}
        className="w-full h-full"
        style={{
          position: "relative",
          // Ensure the viewer fills the container
          minHeight: "100%",
        }}
      />
    </div>
  );
}
