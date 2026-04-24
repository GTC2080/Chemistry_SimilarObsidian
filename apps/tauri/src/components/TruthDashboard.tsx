/**
 * TRUTH_SYSTEM Dashboard — 全维度个人成长看板
 *
 * 极简终端质感 · 高对比度暗色 · 电光蓝雷达
 */

import { useEffect, useRef } from "react";
import {
  RadarChart,
  PolarGrid,
  PolarAngleAxis,
  Radar,
  ResponsiveContainer,
} from "recharts";
import type { TruthState, AttributeKey } from "../models/truth_system";
import { useT } from "../i18n";

/* ========== 学科技能树元数据 ========== */

interface DisciplineAttrMeta {
  key: AttributeKey;
  label: string;
  tagKey: string;
}

interface ChemistryTreeMeta {
  code: string;
  nameKey: string;
  attrs: DisciplineAttrMeta[];
}

const CHEMISTRY_TREE_META: ChemistryTreeMeta = {
  code: "CHEMISTRY_TREE",
  nameKey: "truth.chemTree",
  attrs: [
    { key: "science", label: "PHYSICAL", tagKey: "truth.physChem" },
    { key: "engineering", label: "ORGANIC", tagKey: "truth.orgChem" },
    { key: "creation", label: "INORGANIC", tagKey: "truth.inorgChem" },
    { key: "finance", label: "ANALYTICAL", tagKey: "truth.analyticChem" },
  ],
};

interface TruthDashboardProps {
  open: boolean;
  onClose: () => void;
  state: TruthState;
}

export default function TruthDashboard({ open, onClose, state }: TruthDashboardProps) {
  const t = useT();
  const overlayRef = useRef<HTMLDivElement>(null);
  const treeMeta = CHEMISTRY_TREE_META;

  // Escape 关闭
  useEffect(() => {
    if (!open) return;
    function onKey(e: KeyboardEvent) {
      if (e.key === "Escape") onClose();
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [open, onClose]);

  if (!open) return null;

  const radarData = treeMeta.attrs.map(m => ({
    subject: m.label,
    value: state.attributes[m.key],
    fullMark: Math.max(
      10,
      ...Object.values(state.attributes).map(v => Math.ceil(v * 1.3)),
    ),
  }));

  const levelProgress = state.nextLevelExp > 0
    ? (state.totalExp / state.nextLevelExp) * 100
    : 0;

  return (
    <div
      ref={overlayRef}
      onClick={e => { if (e.target === overlayRef.current) onClose(); }}
      className="fixed inset-0 z-[9999] flex items-center justify-center"
      style={{
        background: "var(--truth-overlay)",
        backdropFilter: "blur(8px) saturate(1.05)",
        animation: "truthFadeIn 0.5s cubic-bezier(0.22,1,0.36,1) both",
      }}
    >
      <div
        className="w-full max-w-[960px] mx-6 rounded-2xl px-8 py-7 border"
        style={{
          animation: "truthSlideUp 0.5s cubic-bezier(0.22,1,0.36,1) both",
          background: "var(--truth-panel-bg)",
          borderColor: "var(--truth-panel-border)",
          boxShadow: "var(--truth-panel-shadow)",
        }}
      >
        {/* ===== Header ===== */}
        <div className="flex items-baseline justify-between mb-8">
          <div className="space-y-2">
            <h1
              className="text-[28px] font-mono tracking-[0.2em] text-[var(--truth-text-primary)] select-none"
              style={{ fontWeight: 300 }}
            >
              TRUTH_SYSTEM{" "}
              <span className="text-[var(--truth-text-quaternary)]">//</span>{" "}
              <span className="text-[var(--truth-text-secondary)]">LEVEL_{String(state.level).padStart(2, "0")}</span>
            </h1>
            <p className="font-mono text-[11px] tracking-wider text-[var(--truth-text-quaternary)]">
              {treeMeta.code} · {t(treeMeta.nameKey)}
            </p>
          </div>
          <button
            type="button"
            onClick={onClose}
            className="font-mono text-[12px] text-[var(--truth-text-quaternary)] hover:text-[var(--truth-text-tertiary)] transition-colors cursor-pointer tracking-widest"
          >
            [ ESC ]
          </button>
        </div>

        {/* ===== Level progress ===== */}
        <div className="mb-8">
          <div className="flex items-center justify-between mb-2">
            <span className="font-mono text-[11px] tracking-wider text-[var(--truth-text-tertiary)]">
              TOTAL EXP
            </span>
            <span className="font-mono text-[11px] tracking-wider text-[var(--truth-text-tertiary)]">
              {state.totalExp} / {state.nextLevelExp}
            </span>
          </div>
          <div className="h-[3px] w-full bg-[var(--truth-track)] overflow-hidden">
            <div
              className="h-full bg-[#3B82F6] transition-all duration-1000 ease-out"
              style={{ width: `${Math.min(100, levelProgress)}%` }}
            />
          </div>
        </div>

        {/* ===== Two-column: Radar + Attributes ===== */}
        <div className="flex gap-12">
          {/* Left: Radar */}
          <div className="w-[380px] h-[340px] shrink-0 -ml-4">
            <ResponsiveContainer width="100%" height="100%">
              <RadarChart data={radarData} cx="50%" cy="50%" outerRadius="75%">
                <PolarGrid
                  stroke="var(--truth-grid)"
                  strokeWidth={0.5}
                />
                <PolarAngleAxis
                  dataKey="subject"
                  tick={{
                    fill: "var(--truth-text-tertiary)",
                    fontSize: 10,
                    fontFamily: "ui-monospace, 'SF Mono', 'Fira Code', Consolas, monospace",
                  }}
                  tickLine={false}
                />
                <Radar
                  name="attributes"
                  dataKey="value"
                  stroke="#3B82F6"
                  strokeWidth={1.5}
                  fill="rgba(59,130,246,0.1)"
                  dot={{
                    r: 3,
                    fill: "var(--truth-text-secondary)",
                    stroke: "#3B82F6",
                    strokeWidth: 1,
                  }}
                />
              </RadarChart>
            </ResponsiveContainer>
          </div>

          {/* Right: Attribute bars */}
          <div className="flex-1 flex flex-col justify-center gap-6">
            {treeMeta.attrs.map(m => {
              const attrExp = state.attributeExp[m.key];
              const attrLvl = state.attributes[m.key];
              // 当前等级内的进度
              const baseExp = (attrLvl - 1) * 50;
              const progress = Math.min(100, ((attrExp - baseExp) / 50) * 100);

              return (
                <div key={m.key}>
                  <div className="flex items-baseline justify-between mb-1">
                    <div className="flex items-baseline gap-3">
                      <span className="font-mono text-[12px] tracking-wider text-[var(--truth-text-tertiary)]">
                        {m.label}
                      </span>
                      <span className="font-mono text-[10px] text-[var(--truth-text-quaternary)]">
                        {t(m.tagKey)}
                      </span>
                    </div>
                    <span className="font-mono text-[12px] tracking-wider text-[var(--truth-text-secondary)]">
                      LV.{String(attrLvl).padStart(2, "0")}
                    </span>
                  </div>
                  <div className="h-[3px] w-full bg-[var(--truth-track)] overflow-hidden mt-1">
                    <div
                      className="h-full bg-[#3B82F6] transition-all duration-1000 ease-out"
                      style={{ width: `${Math.min(100, progress)}%` }}
                    />
                  </div>
                  <div className="flex justify-end mt-1">
                    <span className="font-mono text-[9px] text-[var(--truth-text-quaternary)]">
                      {attrExp} EXP
                    </span>
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* ===== Footer ===== */}
        <div className="mt-10 pt-4 border-t border-[var(--truth-divider)]">
          <p className="font-mono text-[10px] text-[var(--truth-text-quaternary)] tracking-wider text-center select-none">
            NEXUS TRUTH_SYSTEM v0.1 — SILENT OBSERVER PROTOCOL ACTIVE
          </p>
        </div>
      </div>
    </div>
  );
}
