import { useRef, useEffect, useState, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import { useT } from "../i18n";

// ===== Rust 返回的强类型渲染协议 =====

interface Vec3D {
  x: number;
  y: number;
  z: number;
}

interface SymmetryPlane {
  normal: Vec3D;
  center: Vec3D;
  vertices: [Vec3D, Vec3D, Vec3D, Vec3D];
}

interface RotationAxis {
  vector: Vec3D;
  center: Vec3D;
  order: number;
  start: Vec3D;
  end: Vec3D;
}

interface SymmetryData {
  pointGroup: string;
  planes: SymmetryPlane[];
  axes: RotationAxis[];
  hasInversion: boolean;
  atomCount: number;
}

interface SymmetryViewer3DProps {
  data: string;
  format: string;
  filePath: string;
}

type ViewState = "idle" | "loading" | "success" | "error";

export default function SymmetryViewer3D({ data, format }: SymmetryViewer3DProps) {
  const t = useT();
  const containerRef = useRef<HTMLDivElement>(null);
  const viewerRef = useRef<any>(null);
  const requestSeqRef = useRef(0);
  const mountedRef = useRef(true);
  const [viewState, setViewState] = useState<ViewState>("idle");
  const [error, setError] = useState<string | null>(null);
  const [symmetryData, setSymmetryData] = useState<SymmetryData | null>(null);
  const [showPlanes, setShowPlanes] = useState(true);
  const [showAxes, setShowAxes] = useState(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // 调用 Rust 后端计算对称性
  const runAnalysis = useCallback(async () => {
    const requestSeq = ++requestSeqRef.current;
    setViewState("loading");
    setError(null);
    setSymmetryData(null);
    try {
      const result = await invoke<SymmetryData>("calculate_symmetry", {
        data,
        format,
      });
      if (!mountedRef.current || requestSeq !== requestSeqRef.current) return;
      setSymmetryData(result);
      setViewState("success");
    } catch (e) {
      if (!mountedRef.current || requestSeq !== requestSeqRef.current) return;
      setError(e instanceof Error ? e.message : String(e));
      setViewState("error");
    }
  }, [data, format]);

  // 文件切换时重新分析
  useEffect(() => {
    void runAnalysis();
  }, [runAnalysis]);

  // 3Dmol 渲染管线：纯粹的"Dumb Renderer"，只读取 JSON 坐标
  useEffect(() => {
    if (viewState !== "success" || !symmetryData || !containerRef.current) return;

    let destroyed = false;
    let cleanupResize: (() => void) | null = null;

    (async () => {
      try {
        const $3Dmol = await import("3dmol");
        if (destroyed || !containerRef.current) return;

        // 清理前一个 viewer
        if (viewerRef.current) {
          try { viewerRef.current.clear(); } catch { /* ignore */ }
          viewerRef.current = null;
        }
        containerRef.current.innerHTML = "";

        const viewer = $3Dmol.createViewer(containerRef.current, {
          backgroundColor: "#0A0A0A",
          antialias: true,
          cartoonQuality: 8,
        });
        viewerRef.current = viewer;

        // ===== 基础分子渲染 =====
        const formatMap: Record<string, string> = { pdb: "pdb", xyz: "xyz", cif: "cif" };
        viewer.addModel(data, formatMap[format.toLowerCase()] ?? "pdb");

        const atomCount = viewer.getModel(0)?.selectedAtoms({})?.length ?? 0;
        if (atomCount > 500) {
          viewer.setStyle({}, { cartoon: { color: "spectrum", opacity: 0.9 } });
          viewer.setStyle({ hetflag: true }, { stick: { radius: 0.15, colorscheme: "Jmol" } });
        } else {
          viewer.setStyle({}, {
            stick: { radius: 0.14, colorscheme: "Jmol" },
            sphere: { scale: 0.25, colorscheme: "Jmol" },
          });
        }

        // ===== 渲染镜像平面 (Mirror Planes) =====
        // 遍历 Rust 返回的 planes 数组，直接使用预计算的顶点坐标
        if (showPlanes) {
          for (const plane of symmetryData.planes) {
            const [v0, v1, v2, v3] = plane.vertices;
            const custom = {
              vertexArr: [
                { x: v0.x, y: v0.y, z: v0.z },
                { x: v1.x, y: v1.y, z: v1.z },
                { x: v2.x, y: v2.y, z: v2.z },
                { x: v3.x, y: v3.y, z: v3.z },
              ],
              normalArr: [
                { x: plane.normal.x, y: plane.normal.y, z: plane.normal.z },
                { x: plane.normal.x, y: plane.normal.y, z: plane.normal.z },
                { x: plane.normal.x, y: plane.normal.y, z: plane.normal.z },
                { x: plane.normal.x, y: plane.normal.y, z: plane.normal.z },
              ],
              faceArr: [0, 1, 2, 0, 2, 3],
            };

            viewer.addCustom({
              vertexArr: custom.vertexArr,
              normalArr: custom.normalArr,
              faceArr: custom.faceArr,
              color: "#EDEDED",
              alpha: 0.3,
            } as any);
          }
        }

        // ===== 渲染旋转轴 (Rotation Axes) =====
        // 遍历 Rust 返回的 axes 数组，直接使用预计算的端点坐标
        if (showAxes) {
          for (const axis of symmetryData.axes) {
            viewer.addCylinder({
              start: { x: axis.start.x, y: axis.start.y, z: axis.start.z },
              end: { x: axis.end.x, y: axis.end.y, z: axis.end.z },
              radius: 0.1,
              color: "#3B82F6",
              fromCap: 1,
              toCap: 1,
            });
          }
        }

        viewer.zoomTo();
        viewer.render();
        viewer.zoom(0.85, 300);

        // Resize 监听
        const handleResize = () => {
          if (!viewerRef.current) return;
          try {
            viewerRef.current.resize();
            viewerRef.current.render();
          } catch { /* ignore */ }
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
      // 内存泄漏阻断：彻底销毁 WebGL context
      if (viewerRef.current) {
        try {
          viewerRef.current.clear();
          viewerRef.current = null;
        } catch { /* ignore */ }
      }
      if (containerRef.current) {
        containerRef.current.innerHTML = "";
      }
    };
  }, [symmetryData, data, format, showPlanes, showAxes, viewState]);

  // ===== Loading 骨架屏 =====
  if (viewState === "loading") {
    return (
      <div className="flex-1 flex items-center justify-center" style={{ background: "#0A0A0A" }}>
        <div className="flex flex-col items-center gap-4">
          <div className="w-6 h-6 border-2 border-[#3B82F6] border-t-transparent rounded-full animate-spin" />
          <span className="text-sm text-[#666] font-mono">{t("symmetry.computing")}</span>
        </div>
      </div>
    );
  }

  // ===== 错误状态 =====
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
          <p className="text-sm text-[#888] mb-2">{t("symmetry.failed")}</p>
          <p className="text-xs text-[#555] font-mono break-all">{error}</p>
          <button
            type="button"
            onClick={() => void runAnalysis()}
            className="mt-4 px-4 py-1.5 rounded-md text-xs font-medium cursor-pointer
              bg-[rgba(59,130,246,0.15)] border border-[rgba(59,130,246,0.3)] text-[#93b8f5]
              hover:bg-[rgba(59,130,246,0.25)] transition-colors"
          >
            {t("symmetry.retry")}
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="flex-1 relative" style={{ background: "#0A0A0A" }}>
      {/* ===== 点群 HUD ===== */}
      {symmetryData && (
        <div className="absolute top-3 left-3 z-20 flex flex-col gap-2">
          {/* 点群符号 */}
          <div className="px-3 py-2 rounded-lg bg-[rgba(0,0,0,0.7)] border border-[rgba(255,255,255,0.08)]">
            <span className="text-[20px] font-mono font-bold text-[#EDEDED] tracking-wide">
              {symmetryData.pointGroup}
            </span>
          </div>

          {/* 对称元素统计 */}
          <div className="px-3 py-1.5 rounded-lg bg-[rgba(0,0,0,0.5)] border border-[rgba(255,255,255,0.06)]">
            <div className="flex flex-col gap-1 text-[10px] font-mono text-[#777]">
              {symmetryData.axes.length > 0 && (
                <span>
                  {symmetryData.axes.map(a => `C${a.order === 0 ? "∞" : a.order}`).join(", ")}
                </span>
              )}
              {symmetryData.planes.length > 0 && (
                <span>σ × {symmetryData.planes.length}</span>
              )}
              {symmetryData.hasInversion && <span>i</span>}
            </div>
          </div>
        </div>
      )}

      {/* ===== 控制面板 ===== */}
      <div className="absolute top-3 right-3 z-20 flex items-center gap-2">
        <button
          type="button"
          onClick={() => setShowPlanes(p => !p)}
          className={`px-2.5 py-1 rounded-md text-[11px] font-medium cursor-pointer transition-colors border
            ${showPlanes
              ? "bg-[rgba(237,237,237,0.15)] border-[rgba(237,237,237,0.3)] text-[#EDEDED]"
              : "bg-transparent border-[rgba(255,255,255,0.08)] text-[#555]"
            }`}
        >
          {t("symmetry.mirrorPlane")}
        </button>
        <button
          type="button"
          onClick={() => setShowAxes(a => !a)}
          className={`px-2.5 py-1 rounded-md text-[11px] font-medium cursor-pointer transition-colors border
            ${showAxes
              ? "bg-[rgba(59,130,246,0.2)] border-[rgba(59,130,246,0.4)] text-[#93b8f5]"
              : "bg-transparent border-[rgba(255,255,255,0.08)] text-[#555]"
            }`}
        >
          {t("symmetry.rotationAxis")}
        </button>
        <span className="px-2.5 py-1 rounded-md text-[11px] bg-[rgba(0,0,0,0.45)] border border-[rgba(255,255,255,0.08)] text-[#555]">
          {symmetryData?.atomCount ?? 0} atoms
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
