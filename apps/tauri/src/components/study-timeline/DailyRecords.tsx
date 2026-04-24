import { useState, useMemo, useCallback } from "react";
import { formatDuration } from "./StatsCards";
import type { DailyDetail } from "./types";
import { useT } from "../../i18n";

interface DailyRecordsProps {
  data: DailyDetail[];
}

function getTodayStr(): string {
  const d = new Date();
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
}

function getYesterdayStr(): string {
  const d = new Date();
  d.setDate(d.getDate() - 1);
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
}

function ChevronRight() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <path d="M5.5 3L9.5 7L5.5 11" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

function ChevronDown() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <path d="M3 5.5L7 9.5L11 5.5" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

export default function DailyRecords({ data }: DailyRecordsProps) {
  const t = useT();
  const dayNames = [t("timeline.daySun"), t("timeline.dayMon"), t("timeline.dayTue"), t("timeline.dayWed"), t("timeline.dayThu"), t("timeline.dayFri"), t("timeline.daySat")];

  const formatDateLabel = useCallback((dateStr: string): string => {
    const d = new Date(dateStr + "T00:00:00");
    const dayName = dayNames[d.getDay()];
    const mm = String(d.getMonth() + 1).padStart(2, "0");
    const dd = String(d.getDate()).padStart(2, "0");
    return `${mm}-${dd} ${t("timeline.weekPrefix")}${dayName}`;
  }, [dayNames, t]);

  const today = getTodayStr();
  const yesterday = getYesterdayStr();

  // Sort newest first
  const sorted = useMemo(
    () => [...data].sort((a, b) => b.date.localeCompare(a.date)),
    [data]
  );

  const [expanded, setExpanded] = useState<Set<string>>(() => {
    const set = new Set<string>();
    for (const d of sorted) {
      if (d.date === today || d.date === yesterday) {
        set.add(d.date);
      }
    }
    return set;
  });

  const toggle = (date: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(date)) {
        next.delete(date);
      } else {
        next.add(date);
      }
      return next;
    });
  };

  if (sorted.length === 0) {
    return (
      <div
        className="text-[12px] py-8 text-center"
        style={{ color: "var(--text-quaternary)" }}
      >
        {t("timeline.noRecords")}
      </div>
    );
  }

  return (
    <div className="flex flex-col gap-1.5">
      <div className="text-[11px] font-medium mb-1" style={{ color: "var(--text-quaternary)" }}>
        {t("timeline.dailyRecords")}
      </div>
      {sorted.map((day) => {
        const totalSecs = day.files.reduce((acc, f) => acc + f.active_secs, 0);
        const isOpen = expanded.has(day.date);
        return (
          <div
            key={day.date}
            className="rounded-lg overflow-hidden"
            style={{ border: "1px solid var(--separator-light)" }}
          >
            <button
              type="button"
              className="w-full flex items-center gap-2 px-3 py-2.5 text-left transition-colors"
              style={{
                background: isOpen ? "var(--subtle-surface)" : "transparent",
                cursor: "pointer",
              }}
              onClick={() => toggle(day.date)}
            >
              <span style={{ color: "var(--text-quaternary)" }}>
                {isOpen ? <ChevronDown /> : <ChevronRight />}
              </span>
              <span
                className="flex-1 text-[12px] font-medium"
                style={{ color: "var(--text-secondary)" }}
              >
                {formatDateLabel(day.date)}
              </span>
              <span
                className="text-[11px] tabular-nums"
                style={{ color: "var(--text-quaternary)" }}
              >
                {formatDuration(totalSecs)}
              </span>
              <span
                className="text-[11px] tabular-nums ml-1"
                style={{ color: "var(--text-quaternary)" }}
              >
                {day.files.length} {t("timeline.files")}
              </span>
            </button>

            {isOpen && day.files.length > 0 && (
              <div
                className="flex flex-col divide-y divide-white/5"
                style={{ borderTop: "1px solid var(--separator-light)" }}
              >
                {day.files.map((file) => {
                  const name = file.note_id.split("/").pop() ?? file.note_id;
                  return (
                    <div
                      key={file.note_id}
                      className="flex items-center gap-2 px-4 py-2"
                      style={{ background: "var(--subtle-surface)" }}
                    >
                      <div className="flex-1 min-w-0">
                        <div
                          className="text-[12px] truncate"
                          style={{ color: "var(--text-secondary)" }}
                          title={file.note_id}
                        >
                          {name}
                        </div>
                        {file.folder && (
                          <div
                            className="text-[10px] truncate mt-0.5"
                            style={{ color: "var(--text-quaternary)" }}
                          >
                            {file.folder}
                          </div>
                        )}
                      </div>
                      <div
                        className="shrink-0 text-[11px] tabular-nums"
                        style={{ color: "var(--text-quaternary)" }}
                      >
                        {formatDuration(file.active_secs)}
                      </div>
                    </div>
                  );
                })}
              </div>
            )}

            {isOpen && day.files.length === 0 && (
              <div
                className="px-4 py-3 text-[11px]"
                style={{ color: "var(--text-quaternary)", borderTop: "1px solid var(--separator-light)" }}
              >
                {t("timeline.noFileRecords")}
              </div>
            )}
          </div>
        );
      })}
    </div>
  );
}
