import { useRef, useEffect, useState, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import { useT } from "../i18n";

// ===== Rust 返回的强类型渲染协议 =====

interface UnitCellBox {
  a: number;
  b: number;
  c: number;
  alpha: number;
  beta: number;
  gamma: number;
  origin: [number, number, number];
  vectors: [[number, number, number], [number, number, number], [number, number, number]];
}

interface AtomNode {
  element: string;
  cartesianCoords: [number, number, number];
}

interface LatticeData {
  unitCell: UnitCellBox;
  atoms: AtomNode[];
}

interface MillerPlaneData {
  normal: [number, number, number];
  center: [number, number, number];
  d: number;
  vertices: [[number, number, number], [number, number, number], [number, number, number], [number, number, number]];
}

interface CrystalViewer3DProps {
  data: string;
  format: string;
  filePath: string;
}

type ViewState = "idle" | "loading" | "success" | "error";

// 元素颜色映射（CPK）
const ELEMENT_COLORS: Record<string, string> = {
  H: "#FFFFFF", He: "#D9FFFF", Li: "#CC80FF", Be: "#C2FF00", B: "#FFB5B5",
  C: "#909090", N: "#3050F8", O: "#FF0D0D", F: "#90E050", Ne: "#B3E3F5",
  Na: "#AB5CF2", Mg: "#8AFF00", Al: "#BFA6A6", Si: "#F0C8A0", P: "#FF8000",
  S: "#FFFF30", Cl: "#1FF01F", Ar: "#80D1E3", K: "#8F40D4", Ca: "#3DFF00",
  Ti: "#BFC2C7", Fe: "#E06633", Co: "#F090A0", Ni: "#50D050", Cu: "#C88033",
  Zn: "#7D80B0", Br: "#A62929", Ag: "#C0C0C0", Au: "#FFD123", Pt: "#D0D0E0",
};

function getElementColor(el: string): string {
  return ELEMENT_COLORS[el] ?? "#EDEDED";
}

export default function CrystalViewer3D({ data, format }: CrystalViewer3DProps) {
  const t = useT();
  const containerRef = useRef<HTMLDivElement>(null);
  const viewerRef = useRef<any>(null);
  const requestSeqRef = useRef(0);
  const mountedRef = useRef(true);

  const [viewState, setViewState] = useState<ViewState>("idle");
  const [error, setError] = useState<string | null>(null);
  const [latticeData, setLatticeData] = useState<LatticeData | null>(null);
  const [millerPlane, setMillerPlane] = useState<MillerPlaneData | null>(null);

  // 超晶胞维度
  const [nx, setNx] = useState(1);
  const [ny, setNy] = useState(1);
  const [nz, setNz] = useState(1);

  // 密勒指数
  const [millerH, setMillerH] = useState(1);
  const [millerK, setMillerK] = useState(0);
  const [millerL, setMillerL] = useState(0);

  // 显示控制
  const [showCell, setShowCell] = useState(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // 构建晶格
  const buildLattice = useCallback(async () => {
    if (format.toLowerCase() !== "cif") return;
    const seq = ++requestSeqRef.current;
    setViewState("loading");
    setError(null);
    setLatticeData(null);
    setMillerPlane(null);
    try {
      const result = await invoke<LatticeData>("parse_and_build_lattice", {
        cifText: data,
        nx,
        ny,
        nz,
      });
      if (!mountedRef.current || seq !== requestSeqRef.current) return;
      setLatticeData(result);
      setViewState("success");
    } catch (e) {
      if (!mountedRef.current || seq !== requestSeqRef.current) return;
      setError(e instanceof Error ? e.message : String(e));
      setViewState("error");
    }
  }, [data, format, nx, ny, nz]);

  useEffect(() => {
    void buildLattice();
  }, [buildLattice]);

  // 密勒切割
  const handleSlice = useCallback(async () => {
    if (millerH === 0 && millerK === 0 && millerL === 0) return;
    try {
      const plane = await invoke<MillerPlaneData>("calculate_miller_plane", {
        cifText: data,
        h: millerH,
        k: millerK,
        l: millerL,
      });
      if (mountedRef.current) setMillerPlane(plane);
    } catch (e) {
      console.error("Miller plane error:", e);
    }
  }, [data, millerH, millerK, millerL]);

  // 3Dmol 渲染管线
  useEffect(() => {
    if (viewState !== "success" || !latticeData || !containerRef.current) return;

    let destroyed = false;
    let cleanupResize: (() => void) | null = null;

    (async () => {
      try {
        const $3Dmol = await import("3dmol");
        if (destroyed || !containerRef.current) return;

        if (viewerRef.current) {
          try { viewerRef.current.clear(); } catch { /* ignore */ }
          viewerRef.current = null;
        }
        containerRef.current.innerHTML = "";

        const viewer = $3Dmol.createViewer(containerRef.current, {
          backgroundColor: "#0A0A0A",
          antialias: true,
        });
        viewerRef.current = viewer;

        const { atoms, unitCell } = latticeData;

        // ===== 渲染原子（球体）=====
        for (const atom of atoms) {
          const [x, y, z] = atom.cartesianCoords;
          viewer.addSphere({
            center: { x, y, z },
            radius: 0.4,
            color: getElementColor(atom.element),
          });
        }

        // ===== 渲染晶胞边界线框 =====
        if (showCell) {
          const [va, vb, vc] = unitCell.vectors;
          // 绘制 nX × nY × nZ 的超晶胞线框
          for (let ix = 0; ix <= nx; ix++) {
            for (let iy = 0; iy <= ny; iy++) {
              for (let iz = 0; iz <= nz; iz++) {
                const o = [
                  ix * va[0] + iy * vb[0] + iz * vc[0],
                  ix * va[1] + iy * vb[1] + iz * vc[1],
                  ix * va[2] + iy * vb[2] + iz * vc[2],
                ];
                // Draw edges from this corner (only if within bounds)
                if (ix < nx) drawLine(viewer, o, add(o, va));
                if (iy < ny) drawLine(viewer, o, add(o, vb));
                if (iz < nz) drawLine(viewer, o, add(o, vc));
              }
            }
          }
        }

        // ===== 渲染密勒切割面 =====
        if (millerPlane) {
          const [v0, v1, v2, v3] = millerPlane.vertices;
          const n = millerPlane.normal;
          viewer.addCustom({
            vertexArr: [
              { x: v0[0], y: v0[1], z: v0[2] },
              { x: v1[0], y: v1[1], z: v1[2] },
              { x: v2[0], y: v2[1], z: v2[2] },
              { x: v3[0], y: v3[1], z: v3[2] },
            ],
            normalArr: [
              { x: n[0], y: n[1], z: n[2] },
              { x: n[0], y: n[1], z: n[2] },
              { x: n[0], y: n[1], z: n[2] },
              { x: n[0], y: n[1], z: n[2] },
            ],
            faceArr: [0, 1, 2, 0, 2, 3],
            color: "#3B82F6",
            alpha: 0.4,
          } as any);
        }

        viewer.zoomTo();
        viewer.render();
        viewer.zoom(0.85, 300);

        // Resize
        const handleResize = () => {
          if (!viewerRef.current) return;
          try { viewerRef.current.resize(); viewerRef.current.render(); } catch { /* */ }
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
      } catch (e) {
        if (!destroyed) {
          setError(e instanceof Error ? e.message : String(e));
          setViewState("error");
        }
      }
    })();

    return () => {
      destroyed = true;
      cleanupResize?.();
      if (viewerRef.current) {
        try { viewerRef.current.clear(); viewerRef.current = null; } catch { /* */ }
      }
      if (containerRef.current) {
        containerRef.current.innerHTML = "";
      }
    };
  }, [latticeData, millerPlane, showCell, viewState, nx, ny, nz]);

  // Loading 状态
  if (viewState === "loading") {
    return (
      <div className="flex-1 flex items-center justify-center" style={{ background: "#0A0A0A" }}>
        <div className="flex flex-col items-center gap-4">
          <div className="w-6 h-6 border-2 border-[#3B82F6] border-t-transparent rounded-full animate-spin" />
          <span className="text-sm text-[#666] font-mono">{t("crystal.loading")}</span>
        </div>
      </div>
    );
  }

  // 非 CIF 格式
  if (format.toLowerCase() !== "cif") {
    return (
      <div className="flex-1 flex items-center justify-center" style={{ background: "#0A0A0A" }}>
        <span className="text-sm text-[#555] font-mono">Crystal view requires .cif format</span>
      </div>
    );
  }

  // 错误状态
  if (viewState === "error") {
    return (
      <div className="flex-1 flex items-center justify-center" style={{ background: "#0A0A0A" }}>
        <div className="text-center px-8 max-w-md">
          <svg className="mx-auto w-10 h-10 mb-4 text-[#444]"
            viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5"
            strokeLinecap="round" strokeLinejoin="round">
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
          <p className="text-sm text-[#888] mb-2">{t("crystal.failed")}</p>
          <p className="text-xs text-[#555] font-mono break-all">{error}</p>
          <button
            type="button"
            onClick={() => void buildLattice()}
            className="mt-4 px-4 py-1.5 rounded-md text-xs font-medium cursor-pointer
              bg-[rgba(59,130,246,0.15)] border border-[rgba(59,130,246,0.3)] text-[#93b8f5]
              hover:bg-[rgba(59,130,246,0.25)] transition-colors"
          >
            {t("crystal.retry")}
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="flex-1 relative" style={{ background: "#0A0A0A" }}>
      {/* ===== 左上角控制台：超晶胞 + 密勒指数 ===== */}
      <div className="absolute top-3 left-3 z-20 flex flex-col gap-2">
        {/* 超晶胞维度 */}
        <div className="px-3 py-2 rounded-lg bg-[rgba(0,0,0,0.7)] border border-[rgba(255,255,255,0.08)]">
          <div className="text-[10px] font-mono text-[#555] mb-1.5 uppercase tracking-wider">
            {t("crystal.supercell")}
          </div>
          <div className="flex items-center gap-1">
            {[
              { val: nx, set: setNx },
              { val: ny, set: setNy },
              { val: nz, set: setNz },
            ].map((dim, i) => (
              <div key={i} className="flex items-center">
                {i > 0 && <span className="text-[#333] text-[10px] mx-0.5">&times;</span>}
                <input
                  type="number"
                  min={1}
                  max={5}
                  value={dim.val}
                  onChange={e => dim.set(Math.max(1, Math.min(5, Number(e.target.value) || 1)))}
                  className="bg-[#141414] border border-[#333] text-[#EDEDED] font-mono w-10 text-center
                    text-[12px] rounded px-1 py-0.5 outline-none focus:border-[#3B82F6] transition-colors
                    [appearance:textfield] [&::-webkit-inner-spin-button]:appearance-none [&::-webkit-outer-spin-button]:appearance-none"
                />
              </div>
            ))}
          </div>
        </div>

        {/* 密勒指数切割器 */}
        <div className="px-3 py-2 rounded-lg bg-[rgba(0,0,0,0.7)] border border-[rgba(255,255,255,0.08)]">
          <div className="text-[10px] font-mono text-[#555] mb-1.5 uppercase tracking-wider">
            {t("crystal.millerSlice")}
          </div>
          <div className="flex items-center gap-1.5">
            {[
              { label: "h", val: millerH, set: setMillerH },
              { label: "k", val: millerK, set: setMillerK },
              { label: "l", val: millerL, set: setMillerL },
            ].map(m => (
              <input
                key={m.label}
                type="number"
                min={-9}
                max={9}
                value={m.val}
                onChange={e => m.set(Number(e.target.value) || 0)}
                title={m.label}
                className="bg-[#141414] border border-[#333] text-[#EDEDED] font-mono w-10 text-center
                  text-[12px] rounded px-1 py-0.5 outline-none focus:border-[#3B82F6] transition-colors
                  [appearance:textfield] [&::-webkit-inner-spin-button]:appearance-none [&::-webkit-outer-spin-button]:appearance-none"
              />
            ))}
            <button
              type="button"
              onClick={() => void handleSlice()}
              disabled={millerH === 0 && millerK === 0 && millerL === 0}
              className="px-2.5 py-0.5 rounded text-[11px] font-medium cursor-pointer transition-colors
                bg-[rgba(59,130,246,0.2)] border border-[rgba(59,130,246,0.4)] text-[#93b8f5]
                hover:bg-[rgba(59,130,246,0.3)] disabled:opacity-40 disabled:cursor-not-allowed"
            >
              {t("crystal.slice")}
            </button>
          </div>
          {millerPlane && (
            <div className="mt-1.5 text-[10px] font-mono text-[#555]">
              ({millerH} {millerK} {millerL}) d = {Math.abs(1 / Math.sqrt(
                millerPlane.normal[0] ** 2 + millerPlane.normal[1] ** 2 + millerPlane.normal[2] ** 2
              )).toFixed(3)} A
            </div>
          )}
        </div>
      </div>

      {/* ===== 右上角状态 HUD ===== */}
      <div className="absolute top-3 right-3 z-20 flex items-center gap-2">
        <button
          type="button"
          onClick={() => setShowCell(v => !v)}
          className={`px-2.5 py-1 rounded-md text-[11px] font-medium cursor-pointer transition-colors border
            ${showCell
              ? "bg-[rgba(237,237,237,0.15)] border-[rgba(237,237,237,0.3)] text-[#EDEDED]"
              : "bg-transparent border-[rgba(255,255,255,0.08)] text-[#555]"
            }`}
        >
          {t("crystal.showCell")}
        </button>
        <span className="px-2.5 py-1 rounded-md text-[11px] bg-[rgba(0,0,0,0.45)] border border-[rgba(255,255,255,0.08)] text-[#555]">
          {latticeData?.atoms.length ?? 0} {t("crystal.atoms")}
        </span>
      </div>

      {/* ===== 3D 画布 ===== */}
      <div
        ref={containerRef}
        className="w-full h-full"
        style={{ position: "relative", minHeight: "100%" }}
      />
    </div>
  );
}

// ===== 工具函数 =====

function add(a: number[], b: number[]): number[] {
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function drawLine(viewer: any, from: number[], to: number[]) {
  viewer.addCylinder({
    start: { x: from[0], y: from[1], z: from[2] },
    end: { x: to[0], y: to[1], z: to[2] },
    radius: 0.03,
    color: "#888888",
    fromCap: 0,
    toCap: 0,
  });
}
