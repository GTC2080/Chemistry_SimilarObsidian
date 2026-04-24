import type { StudyStats } from "./types";
import { useT } from "../../i18n";

interface StatsCardsProps {
  stats: StudyStats;
}

export function formatDuration(secs: number): string {
  if (secs <= 0) return "0s";
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m} min`;
  return `${secs}s`;
}

interface StatCardProps {
  label: string;
  value: string;
}

function StatCard({ label, value }: StatCardProps) {
  return (
    <div
      className="flex-1 rounded-xl px-5 py-5 border"
      style={{
        background: "var(--subtle-surface)",
        borderColor: "var(--separator-light)",
      }}
    >
      <div
        className="text-[10px] font-semibold uppercase tracking-wider mb-2.5"
        style={{ color: "var(--text-quaternary)" }}
      >
        {label}
      </div>
      <div
        className="font-bold leading-none tabular-nums"
        style={{ fontSize: 24, color: "var(--text-primary)" }}
      >
        {value}
      </div>
    </div>
  );
}

export default function StatsCards({ stats }: StatsCardsProps) {
  const t = useT();
  return (
    <div className="flex gap-4">
      <StatCard label={t("timeline.todayStudy")} value={formatDuration(stats.today_active_secs)} />
      <StatCard label={t("timeline.todayFiles")} value={String(stats.today_files)} />
      <StatCard label={t("timeline.weekTotal")} value={formatDuration(stats.week_active_secs)} />
      <StatCard
        label={t("timeline.streak")}
        value={`${stats.streak_days} ${t("timeline.days")}`}
      />
    </div>
  );
}
