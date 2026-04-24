import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { KeyboardEvent as ReactKeyboardEvent } from "react";
import { invoke } from "@tauri-apps/api/core";
import { NodeViewWrapper } from "@tiptap/react";
import type { NodeViewProps } from "@tiptap/react";
import { useT } from "../../i18n";
import {
  createDefaultStoichiometryRows,
  normalizeStoichiometryRows,
  recalculateStoichiometryRows,
  type StoichiometryRow,
} from "../../editor/schema/stoichiometry";

interface CompoundInfo {
  name: string;
  formula: string;
  molecular_weight: number;
  density: number | null;
}

const blockedKeys = new Set(["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Enter", "Tab"]);

function stopEditorNavigation(e: ReactKeyboardEvent<HTMLElement>) {
  if (blockedKeys.has(e.key)) {
    e.stopPropagation();
  }
}

function toNumber(value: string, fallback = 0): number {
  const num = Number(value);
  return Number.isFinite(num) ? num : fallback;
}

function formatNumber(value: number, precision = 3): string {
  if (!Number.isFinite(value) || value <= 0) return "0";
  const fixed = value.toFixed(precision);
  return fixed.replace(/\.?0+$/, "");
}

function createRow(): StoichiometryRow {
  return recalculateStoichiometryRows([
    {
      ...createDefaultStoichiometryRows()[0],
      id: `sto_${Math.random().toString(36).slice(2, 10)}`,
      isReference: false,
      eq: 1,
      moles: 0,
      mass: 0,
      volume: 0,
      name: "",
      formula: "",
      mw: 0,
      density: undefined,
    },
  ])[0];
}

export default function StoichiometryGrid({ node, updateAttributes }: NodeViewProps) {
  const t = useT();
  const rows = useMemo(
    () => normalizeStoichiometryRows((node.attrs as { rows?: unknown }).rows),
    [node.attrs]
  );

  const rowsRef = useRef(rows);
  const timersRef = useRef<Record<string, ReturnType<typeof setTimeout>>>({});
  const requestIdRef = useRef<Record<string, number>>({});
  const [loadingByRow, setLoadingByRow] = useState<Record<string, boolean>>({});

  useEffect(() => {
    rowsRef.current = rows;
  }, [rows]);

  useEffect(() => {
    return () => {
      Object.values(timersRef.current).forEach(timer => clearTimeout(timer));
    };
  }, []);

  const syncRows = useCallback(
    (nextRows: StoichiometryRow[]) => {
      updateAttributes({ rows: recalculateStoichiometryRows(nextRows) });
    },
    [updateAttributes]
  );

  const patchRow = useCallback(
    (rowId: string, updater: (row: StoichiometryRow) => StoichiometryRow) => {
      const nextRows = rowsRef.current.map(row => (row.id === rowId ? updater(row) : row));
      syncRows(nextRows);
    },
    [syncRows]
  );

  const runFetch = useCallback(
    async (rowId: string, rawQuery: string) => {
      const query = rawQuery.trim();
      if (!query) return;

      const requestId = (requestIdRef.current[rowId] ?? 0) + 1;
      requestIdRef.current[rowId] = requestId;
      setLoadingByRow(prev => ({ ...prev, [rowId]: true }));

      try {
        const info = await invoke<CompoundInfo>("fetch_compound_info", { query });
        if (requestIdRef.current[rowId] !== requestId) return;
        patchRow(rowId, row => ({
          ...row,
          name: query,
          formula: info.formula ?? "",
          mw: Number.isFinite(info.molecular_weight) ? Math.max(0, info.molecular_weight) : row.mw,
          density: info.density && info.density > 0 ? info.density : undefined,
        }));
      } catch {
        // 降级容错：抓取失败保持静默，允许手动输入
      } finally {
        if (requestIdRef.current[rowId] === requestId) {
          setLoadingByRow(prev => {
            const next = { ...prev };
            delete next[rowId];
            return next;
          });
        }
      }
    },
    [patchRow]
  );

  const scheduleFetch = useCallback(
    (rowId: string, query: string) => {
      const prevTimer = timersRef.current[rowId];
      if (prevTimer) clearTimeout(prevTimer);

      const trimmed = query.trim();
      if (!trimmed) return;

      timersRef.current[rowId] = setTimeout(() => {
        void runFetch(rowId, trimmed);
      }, 420);
    },
    [runFetch]
  );

  const flushFetch = useCallback(
    (rowId: string, query: string) => {
      const prevTimer = timersRef.current[rowId];
      if (prevTimer) clearTimeout(prevTimer);
      void runFetch(rowId, query);
    },
    [runFetch]
  );

  const setReferenceRow = (rowId: string) => {
    const nextRows = rowsRef.current.map(row => ({
      ...row,
      isReference: row.id === rowId,
      eq: row.id === rowId ? 1 : row.eq,
    }));
    syncRows(nextRows);
  };

  const addRow = () => {
    syncRows([...rowsRef.current, createRow()]);
  };

  const removeRow = (rowId: string) => {
    if (rowsRef.current.length <= 1) return;
    syncRows(rowsRef.current.filter(row => row.id !== rowId));
  };

  return (
    <NodeViewWrapper
      contentEditable={false}
      className="my-6 border border-[#333333] rounded-md overflow-hidden bg-[#0A0A0A]"
      onKeyDownCapture={stopEditorNavigation}
    >
      <div className="overflow-x-auto">
        <table className="w-full table-fixed border-collapse">
          <thead className="bg-[#141414]">
            <tr>
              {["Compound", "Formula", "Mw", "Eq", "mmol", "Mass(mg)", "Vol(μL)"].map(label => (
                <th
                  key={label}
                  className="text-left text-xs font-mono text-[#888888] tracking-wider p-2 border-b border-[#232323]"
                >
                  {label}
                </th>
              ))}
              <th className="w-[34px] border-b border-[#232323]" />
            </tr>
          </thead>
          <tbody>
            {rows.map(row => (
              <tr key={row.id} className="border-b border-[#1E1E1E] last:border-b-0">
                <td className="p-0">
                  <div className="h-full flex items-center gap-2 px-2">
                    <button
                      type="button"
                      className={`w-3 h-3 rounded-full border shrink-0 transition-colors ${
                        row.isReference
                          ? "border-blue-400 bg-blue-400"
                          : "border-[#555555] bg-transparent"
                      }`}
                      title={t("stoich.setReference")}
                      onClick={() => setReferenceRow(row.id)}
                    />
                    <div className="relative flex-1">
                      <input
                        value={row.name}
                        onChange={e => {
                          patchRow(row.id, prev => ({ ...prev, name: e.target.value }));
                          scheduleFetch(row.id, e.target.value);
                        }}
                        onBlur={e => flushFetch(row.id, e.target.value)}
                        onKeyDown={stopEditorNavigation}
                        className="bg-transparent focus:bg-[#1A1A1A] text-sm text-[#EDEDED] outline-none w-full h-full p-2 transition-colors"
                      />
                      {loadingByRow[row.id] && (
                        <span className="absolute right-1.5 top-1/2 -translate-y-1/2 w-1.5 h-1.5 bg-blue-500 rounded-full animate-pulse" />
                      )}
                    </div>
                  </div>
                </td>

                <td className="p-0">
                  <input
                    value={row.formula}
                    onChange={e => patchRow(row.id, prev => ({ ...prev, formula: e.target.value }))}
                    onKeyDown={stopEditorNavigation}
                    className="bg-transparent focus:bg-[#1A1A1A] text-sm text-[#EDEDED] outline-none w-full h-full p-2 transition-colors"
                  />
                </td>

                <td className="p-0">
                  <input
                    value={row.mw ? String(row.mw) : ""}
                    onChange={e => patchRow(row.id, prev => ({ ...prev, mw: Math.max(0, toNumber(e.target.value)) }))}
                    onKeyDown={stopEditorNavigation}
                    className="bg-transparent focus:bg-[#1A1A1A] text-sm text-[#EDEDED] outline-none w-full h-full p-2 transition-colors"
                  />
                </td>

                <td className="p-0">
                  <input
                    value={row.eq ? String(row.eq) : "0"}
                    onChange={e =>
                      patchRow(row.id, prev => ({
                        ...prev,
                        eq: row.isReference ? 1 : Math.max(0, toNumber(e.target.value)),
                      }))
                    }
                    onKeyDown={stopEditorNavigation}
                    readOnly={row.isReference}
                    className="bg-transparent focus:bg-[#1A1A1A] text-sm text-[#EDEDED] outline-none w-full h-full p-2 transition-colors"
                  />
                </td>

                <td className="p-0">
                  <input
                    value={row.moles ? String(row.moles) : "0"}
                    onChange={e => {
                      if (!row.isReference) return;
                      patchRow(row.id, prev => ({ ...prev, moles: Math.max(0, toNumber(e.target.value)) }));
                    }}
                    onKeyDown={stopEditorNavigation}
                    readOnly={!row.isReference}
                    className="bg-transparent focus:bg-[#1A1A1A] text-sm text-[#EDEDED] outline-none w-full h-full p-2 transition-colors"
                  />
                </td>

                <td className="p-2 text-sm text-[#BBBBBB] font-mono">{formatNumber(row.mass, 3)}</td>
                <td className="p-2 text-sm text-[#BBBBBB] font-mono">{formatNumber(row.volume, 2)}</td>
                <td className="text-center">
                  <button
                    type="button"
                    onClick={() => removeRow(row.id)}
                    className="text-xs text-[#666666] hover:text-[#AAAAAA] transition-colors"
                    title={t("stoich.deleteRow")}
                  >
                    x
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <button
        type="button"
        onClick={addRow}
        className="w-full text-left px-3 py-2 text-xs font-mono text-[#777777] hover:text-[#BBBBBB] border-t border-[#1E1E1E] transition-colors"
      >
        + Add Reagent
      </button>
    </NodeViewWrapper>
  );
}
