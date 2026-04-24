import { formatDuration } from "./StatsCards";
import type { FolderRankEntry } from "./types";
import { useT } from "../../i18n";

interface FolderRankingProps {
  data: FolderRankEntry[];
}

export default function FolderRanking({ data }: FolderRankingProps) {
  const t = useT();
  const top5 = data.slice(0, 5);
  const maxSecs = top5[0]?.total_secs ?? 0;

  return (
    <div className="flex flex-col gap-2">
      <div className="text-[11px] font-medium mb-1" style={{ color: "var(--text-quaternary)" }}>
        {t("timeline.topFolders")}
      </div>

      {top5.length === 0 ? (
        <div
          className="text-[12px] py-4 text-center"
          style={{ color: "var(--text-quaternary)" }}
        >
          {t("timeline.noDataYet")}
        </div>
      ) : (
        <div className="flex flex-col gap-2">
          {top5.map((entry) => {
            const ratio = maxSecs > 0 ? entry.total_secs / maxSecs : 0;
            const pct = Math.max(4, Math.round(ratio * 100));
            return (
              <div key={entry.folder} className="flex items-center gap-2">
                <div
                  className="shrink-0 text-[11px] truncate"
                  style={{ width: 90, color: "var(--text-secondary)" }}
                  title={entry.folder}
                >
                  {entry.folder || t("timeline.rootFolder")}
                </div>
                <div className="flex-1 min-w-0 h-2 rounded-full overflow-hidden" style={{ background: "rgba(255,255,255,0.06)" }}>
                  <div
                    className="h-full rounded-full"
                    style={{
                      width: `${pct}%`,
                      background: "linear-gradient(90deg, rgba(10,132,255,0.55) 0%, rgba(10,132,255,0.90) 100%)",
                    }}
                  />
                </div>
                <div
                  className="shrink-0 text-[11px] tabular-nums"
                  style={{ color: "var(--text-quaternary)", minWidth: 48, textAlign: "right" }}
                >
                  {formatDuration(entry.total_secs)}
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
