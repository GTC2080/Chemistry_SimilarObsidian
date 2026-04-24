import { useEffect, useMemo, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import createPlotlyComponent from "react-plotly.js/factory";
import PlotlyBasic from "plotly.js-basic-dist-min";
import type { Config, Data, Layout } from "plotly.js";
import { useDebounce } from "../hooks/useDebounce";

const Plot = createPlotlyComponent(PlotlyBasic as never);

interface KineticsParams {
  m0: number;
  i0: number;
  cta0: number;
  kd: number;
  kp: number;
  kt: number;
  ktr: number;
  timeMax: number;
  steps: number;
}

interface KineticsResult {
  time: number[];
  conversion: number[];
  mn: number[];
  pdi: number[];
}

interface KineticsSimulatorProps {
  onClose: () => void;
}

interface ParamField {
  key: keyof KineticsParams;
  label: string;
  min: number;
  max: number;
  step: number;
}

const PARAM_FIELDS: ParamField[] = [
  { key: "m0", label: "[M]0", min: 0.2, max: 12, step: 0.1 },
  { key: "i0", label: "[I]0", min: 0, max: 2, step: 0.01 },
  { key: "cta0", label: "[CTA]0", min: 0, max: 2, step: 0.01 },
  { key: "kd", label: "kd", min: 0, max: 3, step: 0.01 },
  { key: "kp", label: "kp", min: 0, max: 300, step: 1 },
  { key: "kt", label: "kt", min: 0, max: 300, step: 1 },
  { key: "ktr", label: "ktr", min: 0, max: 50, step: 0.1 },
  { key: "timeMax", label: "t_max", min: 0.5, max: 48, step: 0.1 },
  { key: "steps", label: "steps", min: 100, max: 5000, step: 100 },
];

const DEFAULT_PARAMS: KineticsParams = {
  m0: 5,
  i0: 0.08,
  cta0: 0.15,
  kd: 0.12,
  kp: 65,
  kt: 38,
  ktr: 1.5,
  timeMax: 12,
  steps: 1000,
};

function clamp(value: number, min: number, max: number) {
  return Math.max(min, Math.min(max, value));
}

/** Read CSS custom-property values for Plotly (needs JS color strings, not var refs). */
function readThemeColors() {
  const s = getComputedStyle(document.documentElement);
  const v = (name: string) => s.getPropertyValue(name).trim();
  return {
    textPrimary: v("--text-primary"),
    textSecondary: v("--text-secondary"),
    textTertiary: v("--text-tertiary"),
    textQuaternary: v("--text-quaternary"),
    separator: v("--separator"),
    separatorLight: v("--separator-light"),
    surfaceBg: v("--surface-0"),
  };
}

/** Theme-aware colors for Plotly, re-reads when data-theme attribute changes. */
function usePlotlyTheme() {
  const [colors, setColors] = useState(readThemeColors);

  useEffect(() => {
    const observer = new MutationObserver(() => setColors(readThemeColors()));
    observer.observe(document.documentElement, {
      attributes: true,
      attributeFilter: ["data-theme"],
    });
    return () => observer.disconnect();
  }, []);

  return colors;
}

export default function KineticsSimulator({ onClose }: KineticsSimulatorProps) {
  const [params, setParams] = useState<KineticsParams>(DEFAULT_PARAMS);
  const [debouncedParams, setDebouncedParams] = useState<KineticsParams>(DEFAULT_PARAMS);
  const [result, setResult] = useState<KineticsResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string>("");
  const requestIdRef = useRef(0);
  const theme = usePlotlyTheme();

  const pushDebounced = useDebounce((next: KineticsParams) => setDebouncedParams(next), 150);

  useEffect(() => {
    pushDebounced(params);
  }, [params, pushDebounced]);

  useEffect(() => {
    let cancelled = false;
    const requestId = ++requestIdRef.current;

    (async () => {
      setLoading(true);
      setError("");
      try {
        const payload = {
          ...debouncedParams,
          steps: Math.max(10, Math.round(debouncedParams.steps)),
        };
        const next = await invoke<KineticsResult>("simulate_polymerization", { params: payload });
        if (cancelled || requestId !== requestIdRef.current) return;
        setResult(next);
      } catch (e) {
        if (cancelled || requestId !== requestIdRef.current) return;
        setResult(null);
        setError(e instanceof Error ? e.message : String(e));
      } finally {
        if (!cancelled && requestId === requestIdRef.current) {
          setLoading(false);
        }
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [debouncedParams]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onClose]);

  const conversionFigure = useMemo(() => {
    const traces: Data[] = [
      {
        type: "scatter",
        mode: "lines",
        x: result?.time ?? [],
        y: result?.conversion ?? [],
        line: { color: "#3B82F6", width: 2 },
        name: "Conversion",
        hovertemplate: "t=%{x:.3f}<br>X=%{y:.4f}<extra></extra>",
      },
    ];

    const layout: Partial<Layout> = {
      paper_bgcolor: "transparent",
      plot_bgcolor: "transparent",
      margin: { l: 60, r: 20, t: 24, b: 44 },
      xaxis: {
        title: { text: "Time" },
        color: theme.textQuaternary,
        gridcolor: theme.separatorLight,
        linecolor: theme.separator,
        zerolinecolor: theme.separatorLight,
      },
      yaxis: {
        title: { text: "Conversion" },
        color: theme.textQuaternary,
        gridcolor: theme.separatorLight,
        linecolor: theme.separator,
        zerolinecolor: theme.separatorLight,
        range: [0, 1],
      },
      showlegend: false,
      hoverlabel: {
        bgcolor: theme.surfaceBg,
        bordercolor: theme.separator,
        font: { family: "monospace", color: theme.textPrimary, size: 11 },
      },
    };

    const config: Partial<Config> = {
      responsive: true,
      displaylogo: false,
      scrollZoom: false,
      modeBarButtonsToRemove: ["lasso2d", "select2d", "autoScale2d", "toggleSpikelines"],
    };

    return { traces, layout, config };
  }, [result, theme]);

  const mnPdiFigure = useMemo(() => {
    const traces: Data[] = [
      {
        type: "scatter",
        mode: "lines",
        x: result?.conversion ?? [],
        y: result?.mn ?? [],
        line: { color: theme.textPrimary, width: 2 },
        name: "Mn",
        yaxis: "y",
        hovertemplate: "X=%{x:.4f}<br>Mn=%{y:.4f}<extra></extra>",
      },
      {
        type: "scatter",
        mode: "lines",
        x: result?.conversion ?? [],
        y: result?.pdi ?? [],
        line: { color: "#3B82F6", width: 2 },
        name: "PDI",
        yaxis: "y2",
        hovertemplate: "X=%{x:.4f}<br>PDI=%{y:.4f}<extra></extra>",
      },
    ];

    const layout: Partial<Layout> = {
      paper_bgcolor: "transparent",
      plot_bgcolor: "transparent",
      margin: { l: 60, r: 56, t: 24, b: 44 },
      xaxis: {
        title: { text: "Conversion" },
        color: theme.textQuaternary,
        gridcolor: theme.separatorLight,
        linecolor: theme.separator,
        zerolinecolor: theme.separatorLight,
        range: [0, 1],
      },
      yaxis: {
        title: { text: "Mn" },
        color: theme.textQuaternary,
        gridcolor: theme.separatorLight,
        linecolor: theme.separator,
        zerolinecolor: theme.separatorLight,
      },
      yaxis2: {
        title: { text: "PDI" },
        color: theme.textQuaternary,
        overlaying: "y",
        side: "right",
        linecolor: theme.separator,
        showgrid: false,
      },
      showlegend: true,
      legend: {
        font: { family: "monospace", size: 11, color: theme.textTertiary },
        bgcolor: "transparent",
        orientation: "h",
        y: 1.08,
        x: 0,
      },
      hoverlabel: {
        bgcolor: theme.surfaceBg,
        bordercolor: theme.separator,
        font: { family: "monospace", color: theme.textPrimary, size: 11 },
      },
    };

    const config: Partial<Config> = {
      responsive: true,
      displaylogo: false,
      scrollZoom: false,
      modeBarButtonsToRemove: ["lasso2d", "select2d", "autoScale2d", "toggleSpikelines"],
    };

    return { traces, layout, config };
  }, [result, theme]);

  return (
    <div className="absolute inset-0 z-40 flex h-full w-full bg-[var(--surface-0)] text-[var(--text-primary)]">
      <aside className="w-80 shrink-0 bg-[var(--panel-bg)] border-r border-[var(--panel-border)] p-4 overflow-y-auto">
        <div className="flex items-center justify-between mb-4">
          <h2 className="font-mono text-sm tracking-wide">POLYMER_KINETICS</h2>
          <button
            type="button"
            onClick={onClose}
            className="h-7 px-2 rounded border border-[var(--separator)] text-xs font-mono text-[var(--text-tertiary)] hover:text-[var(--text-primary)] hover:border-[var(--glass-border-hover)]"
          >
            ESC
          </button>
        </div>

        <div className="space-y-3">
          {PARAM_FIELDS.map(field => {
            const value = params[field.key];
            const ratio = (value - field.min) / (field.max - field.min || 1);
            return (
              <div key={field.key} className="space-y-1.5">
                <div className="flex items-center justify-between">
                  <label className="font-mono text-xs text-[var(--text-tertiary)]">{field.label}</label>
                  <input
                    type="number"
                    value={value}
                    min={field.min}
                    max={field.max}
                    step={field.step}
                    onChange={e => {
                      const parsed = Number(e.target.value);
                      if (!Number.isFinite(parsed)) return;
                      setParams(prev => ({
                        ...prev,
                        [field.key]: field.key === "steps" ? Math.round(clamp(parsed, field.min, field.max)) : clamp(parsed, field.min, field.max),
                      }));
                    }}
                    className="w-24 h-7 bg-[var(--surface-0)] border border-[var(--glass-border)] rounded px-2 text-xs font-mono text-[var(--text-primary)] outline-none"
                  />
                </div>

                <input
                  type="range"
                  min={field.min}
                  max={field.max}
                  step={field.step}
                  value={value}
                  onChange={e => {
                    const parsed = Number(e.target.value);
                    setParams(prev => ({
                      ...prev,
                      [field.key]: field.key === "steps" ? Math.round(parsed) : parsed,
                    }));
                  }}
                  className="kinetics-slider w-full h-1.5 rounded"
                  style={{
                    background: `linear-gradient(to right, #3B82F6 0%, #3B82F6 ${(ratio * 100).toFixed(2)}%, var(--surface-3) ${(ratio * 100).toFixed(2)}%, var(--surface-3) 100%)`,
                  }}
                />
              </div>
            );
          })}
        </div>

        <div className="mt-5 pt-3 border-t border-[var(--panel-border)] space-y-1">
          <div className="flex items-center gap-2 text-xs font-mono text-[var(--text-quaternary)]">
            <span>Solver:</span>
            <span className="text-[var(--text-secondary)]">RK4 + Moments</span>
            {loading && <span className="w-1.5 h-1.5 rounded-full bg-blue-500 animate-pulse" />}
          </div>
          {error && <p className="text-xs text-[#B66] font-mono">{error}</p>}
        </div>
      </aside>

      <section className="flex-1 min-w-0 flex flex-col p-4 gap-4">
        <div className="flex-1 min-h-0 rounded border border-[var(--panel-border)] bg-[var(--subtle-surface)] p-2">
          <Plot
            data={conversionFigure.traces}
            layout={conversionFigure.layout}
            config={conversionFigure.config}
            useResizeHandler
            style={{ width: "100%", height: "100%" }}
          />
        </div>
        <div className="flex-1 min-h-0 rounded border border-[var(--panel-border)] bg-[var(--subtle-surface)] p-2">
          <Plot
            data={mnPdiFigure.traces}
            layout={mnPdiFigure.layout}
            config={mnPdiFigure.config}
            useResizeHandler
            style={{ width: "100%", height: "100%" }}
          />
        </div>
      </section>
    </div>
  );
}
