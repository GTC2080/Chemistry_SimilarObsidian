import { lazy, Suspense, useState, useEffect, useMemo } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo, SpectrumData } from "../types";
import type { Config, Data, Layout } from "plotly.js";
import { useT, useLanguage } from "../i18n";
const PlotlySpectrumChart = lazy(() => import("./spectroscopy/PlotlySpectrumChart"));

/** Palette for multi-series: high-contrast colors on dark bg */
const SERIES_COLORS = [
  "#3B82F6", "#10B981", "#F59E0B", "#EF4444", "#8B5CF6",
  "#EC4899", "#06B6D4", "#84CC16", "#F97316", "#6366F1",
  "#14B8A6", "#E879F9", "#EDEDED", "#FB923C", "#22D3EE",
];

interface SpectroscopyViewerProps {
  note: NoteInfo;
}

type ViewerState =
  | { status: "loading" }
  | { status: "error"; message: string }
  | { status: "ready"; data: SpectrumData };

export default function SpectroscopyViewer({ note }: SpectroscopyViewerProps) {
  const t = useT();
  const language = useLanguage();
  const [state, setState] = useState<ViewerState>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    setState({ status: "loading" });

    (async () => {
      try {
        const data = await invoke<SpectrumData>("parse_spectroscopy", { filePath: note.path });
        if (cancelled) return;
        setState({ status: "ready", data });
      } catch (e) {
        if (cancelled) return;
        setState({
          status: "error",
          message: e instanceof Error ? e.message : t("spectroscopy.unknownFormat"),
        });
      }
    })();

    return () => { cancelled = true; };
  }, [note.id, note.path, note.file_extension]);

  const plotConfig = useMemo(() => {
    if (state.status !== "ready") return null;
    const { data } = state;

    const traces: Data[] = data.series.map((s, i) => ({
      x: data.x,
      y: s.y,
      type: "scatter" as const,
      mode: "lines" as const,
      name: s.label,
      line: { color: SERIES_COLORS[i % SERIES_COLORS.length], width: 1.5 },
      hoverinfo: "x+y+name" as const,
    }));

    const axisBase = {
      gridcolor: "#222222",
      gridwidth: 1,
      linecolor: "#444444",
      linewidth: 1,
      tickfont: { family: "monospace", size: 11, color: "#888888" },
      titlefont: { family: "monospace", size: 12, color: "#888888" },
      zerolinecolor: "#333333",
    };

    const yTitle = data.series.length === 1 ? data.series[0].label : "";

    const layout: Partial<Layout> = {
      plot_bgcolor: "transparent",
      paper_bgcolor: "transparent",
      margin: { t: 40, r: 32, b: 56, l: 72 },
      xaxis: {
        ...axisBase,
        title: { text: data.x_label, standoff: 12 },
        ...(data.is_nmr ? { autorange: "reversed" as const } : {}),
      },
      yaxis: {
        ...axisBase,
        title: { text: yTitle, standoff: 12 },
      },
      legend: data.series.length > 1
        ? {
            font: { family: "monospace", size: 11, color: "#888888" },
            bgcolor: "transparent",
            orientation: "h" as const,
            x: 0.5,
            xanchor: "center" as const,
            y: 1.02,
            yanchor: "bottom" as const,
          }
        : undefined,
      showlegend: data.series.length > 1,
      title: data.title
        ? {
            text: data.title,
            font: { family: "monospace", size: 13, color: "#888888" },
            x: 0.5,
            xanchor: "center" as const,
          }
        : undefined,
      dragmode: "zoom",
      hovermode: "closest",
      hoverlabel: {
        bgcolor: "#1A1A1A",
        bordercolor: "#444444",
        font: { family: "monospace", size: 11, color: "#EDEDED" },
      },
      autosize: true,
    };

    const config: Partial<Config> = {
      responsive: true,
      displaylogo: false,
      modeBarButtonsToRemove: ["lasso2d", "select2d", "sendDataToCloud"],
      scrollZoom: true,
      ...(language === "zh-CN"
        ? {
            locale: "zh-CN",
            locales: {
              "zh-CN": {
                dictionary: {
                  "Download plot as a png": "下载为 PNG 图片",
                  "Download plot": "下载图表",
                  "Zoom": "缩放",
                  "Pan": "平移",
                  "Box Select": "框选",
                  "Lasso Select": "套索选择",
                  "Zoom in": "放大",
                  "Zoom out": "缩小",
                  "Autoscale": "自动缩放",
                  "Reset axes": "重置坐标轴",
                  "Toggle Spike Lines": "切换辅助线",
                  "Show closest data on hover": "悬停显示最近数据",
                  "Compare data on hover": "悬停对比数据",
                  "Produced with Plotly": "由 Plotly 生成",
                  "Toggle show closest data on hover": "切换悬停显示最近数据",
                  "Reset": "重置",
                  "Reset view": "重置视图",
                  "Snapshot": "截图",
                },
                format: {
                  days: ["星期日","星期一","星期二","星期三","星期四","星期五","星期六"],
                  shortDays: ["日","一","二","三","四","五","六"],
                  months: ["一月","二月","三月","四月","五月","六月","七月","八月","九月","十月","十一月","十二月"],
                  shortMonths: ["1月","2月","3月","4月","5月","6月","7月","8月","9月","10月","11月","12月"],
                  decimal: ".",
                  thousands: ",",
                },
              },
            },
          }
        : {}),
    };

    return { traces, layout, config };
  }, [state, language, t]);

  if (state.status === "loading") {
    return (
      <div className="flex-1 flex items-center justify-center">
        <div className="flex flex-col items-center gap-3 animate-pulse">
          <div className="w-64 h-40 rounded-lg bg-[var(--subtle-surface)] border border-[var(--separator-light)]" />
          <span className="text-[12px] text-[var(--text-quaternary)]">
            {t("spectroscopy.loading")}
          </span>
        </div>
      </div>
    );
  }

  if (state.status === "error") {
    return (
      <div className="flex-1 flex items-center justify-center">
        <span className="text-[14px] text-[var(--text-quaternary)]">
          {state.message}
        </span>
      </div>
    );
  }

  const { traces, layout, config } = plotConfig!;
  const pointCount = state.data.x.length;
  const seriesCount = state.data.series.length;

  return (
    <div className="flex-1 flex flex-col min-h-0">
      {/* Info bar */}
      <div className="flex items-center gap-4 px-6 py-2 border-b-[0.5px] border-b-[var(--panel-border)]">
        <span className="text-[11px] font-mono text-[var(--text-quaternary)]">
          {t("spectroscopy.dataPoints").replace("X", pointCount.toLocaleString())}
        </span>
        {seriesCount > 1 && (
          <span className="text-[11px] font-mono text-[var(--text-quaternary)]">
            {t("spectroscopy.curves").replace("X", String(seriesCount))}
          </span>
        )}
        {state.data.is_nmr && (
          <span className="text-[10px] px-2 py-0.5 rounded bg-[#3B82F6]/10 text-[#3B82F6] font-mono">
            NMR
          </span>
        )}
        <span className="text-[11px] font-mono text-[var(--text-quaternary)]">
          {state.data.x_label}
        </span>
      </div>
      {/* Plot */}
      <div className="flex-1 min-h-0">
        <Suspense fallback={<div className="h-full w-full bg-[var(--subtle-surface)]" />}>
          <PlotlySpectrumChart
            traces={traces}
            layout={layout}
            config={config}
          />
        </Suspense>
      </div>
    </div>
  );
}
