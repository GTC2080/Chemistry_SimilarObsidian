export interface HeatmapEntry {
  date: string;
  active_secs: number;
}

export interface FolderRankEntry {
  folder: string;
  total_secs: number;
}

export interface DailyDetail {
  date: string;
  files: { note_id: string; folder: string; active_secs: number }[];
}

export interface DailySummary {
  date: string;
  active_secs: number;
  file_count: number;
}

export interface StudyStats {
  today_active_secs: number;
  today_files: number;
  week_active_secs: number;
  streak_days: number;
  daily_summary: DailySummary[];
  daily_details: DailyDetail[];
  folder_ranking: FolderRankEntry[];
  heatmap: HeatmapEntry[];
}
