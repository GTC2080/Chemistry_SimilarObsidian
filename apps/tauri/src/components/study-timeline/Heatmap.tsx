import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { formatDuration } from "./StatsCards";
import { useT } from "../../i18n";

/** Rust 端 get_heatmap_cells 返回的预计算热力图格子 */
interface HeatmapCell {
  date: string;
  secs: number;
  col: number;
  row: number;
}

interface HeatmapGrid {
  cells: HeatmapCell[];
  maxSecs: number;
}

interface TooltipState {
  x: number;
  y: number;
  date: string;
  secs: number;
}

const GAP = 2;
const WEEKS = 26;
const DAYS_PER_WEEK = 7;

function cellColor(secs: number, maxSecs: number): string {
  if (secs <= 0 || maxSecs <= 0) return "rgba(255,255,255,0.04)";
  const ratio = secs / maxSecs;
  if (ratio < 0.15) return "rgba(10,132,255,0.20)";
  if (ratio < 0.35) return "rgba(10,132,255,0.42)";
  if (ratio < 0.65) return "rgba(10,132,255,0.65)";
  return "rgba(10,132,255,0.90)";
}

function formatDisplayDate(dateStr: string): string {
  const d = new Date(dateStr + "T00:00:00");
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(
    d.getDate()
  ).padStart(2, "0")}`;
}

export default function Heatmap() {
  const t = useT();
  const [tooltip, setTooltip] = useState<TooltipState | null>(null);
  const [grid, setGrid] = useState<HeatmapGrid | null>(null);

  // 从 Rust 获取预计算的热力图网格（26 周 x 7 天，含 col/row 坐标）
  useEffect(() => {
    invoke<HeatmapGrid>("get_heatmap_cells")
      .then(setGrid)
      .catch(e => console.error("加载热力图失败:", e));
  }, []);

  if (!grid) return null;

  const { cells, maxSecs } = grid;

  // Responsive: viewBox-based SVG that scales to fill container width
  const CELL = 14;
  const STEP = CELL + GAP;
  const svgWidth = WEEKS * STEP - GAP;
  const svgHeight = DAYS_PER_WEEK * STEP - GAP;

  return (
    <div className="relative">
      <div className="text-[11px] mb-2 font-medium" style={{ color: "var(--text-quaternary)" }}>
        {t("timeline.heatmapTitle")}
      </div>
      <svg
        viewBox={`0 0 ${svgWidth} ${svgHeight}`}
        className="w-full"
        style={{ display: "block", overflow: "visible" }}
        preserveAspectRatio="xMinYMin meet"
      >
        {cells.map((cell) => (
          <rect
            key={cell.date}
            x={cell.col * STEP}
            y={cell.row * STEP}
            width={CELL}
            height={CELL}
            rx={2.5}
            fill={cellColor(cell.secs, maxSecs)}
            style={{ cursor: "default" }}
            onMouseEnter={(e) => {
              setTooltip({
                x: e.clientX,
                y: e.clientY,
                date: cell.date,
                secs: cell.secs,
              });
            }}
            onMouseMove={(e) => {
              setTooltip((prev) =>
                prev ? { ...prev, x: e.clientX, y: e.clientY } : null
              );
            }}
            onMouseLeave={() => setTooltip(null)}
          />
        ))}
      </svg>

      {tooltip && (
        <div
          className="fixed z-50 px-2.5 py-1.5 rounded-lg text-[11px] pointer-events-none"
          style={{
            left: tooltip.x + 12,
            top: tooltip.y - 36,
            background: "var(--surface-2)",
            border: "1px solid var(--separator-light)",
            color: "var(--text-secondary)",
            boxShadow: "0 4px 16px rgba(0,0,0,0.4)",
          }}
        >
          <span style={{ color: "var(--text-primary)" }}>{formatDisplayDate(tooltip.date)}</span>
          <span className="ml-2">{formatDuration(tooltip.secs)}</span>
        </div>
      )}
    </div>
  );
}
