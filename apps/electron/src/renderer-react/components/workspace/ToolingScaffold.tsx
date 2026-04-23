import type { ReactNode } from "react";

export function ToolWorkspaceShell({
  sidebar,
  children
}: {
  sidebar: ReactNode;
  children: ReactNode;
}) {
  return (
    <div className="flex flex-1 min-w-0 min-h-0 tool-workspace">
      <aside
        className="w-[308px] shrink-0 border-r-[0.5px] border-r-[var(--panel-border)] overflow-y-auto workspace-panel tool-sidebar"
      >
        {sidebar}
      </aside>
      <main className="flex-1 min-w-0 min-h-0 overflow-y-auto workspace-panel tool-main">
        {children}
      </main>
    </div>
  );
}

export function ToolSection({
  title,
  subtitle,
  children,
  action
}: {
  title: string;
  subtitle?: string;
  children: ReactNode;
  action?: ReactNode;
}) {
  return (
    <section className="px-4 py-4 border-b-[0.5px] border-b-[var(--panel-border)] tool-section">
      <div className="flex items-start justify-between gap-3 mb-3">
        <div className="min-w-0">
          <h2 className="text-[12px] font-semibold tracking-[0.08em] uppercase text-[var(--text-tertiary)]">{title}</h2>
          {subtitle ? (
            <p className="text-[11px] mt-1 leading-5 text-[var(--text-quaternary)]">{subtitle}</p>
          ) : null}
        </div>
        {action}
      </div>
      {children}
    </section>
  );
}

export function ToolListButton({
  title,
  subtitle,
  active,
  onClick,
  trailing,
  eyebrow
}: {
  title: string;
  subtitle?: string;
  active?: boolean;
  onClick: () => void;
  trailing?: ReactNode;
  eyebrow?: string;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      className={`w-full text-left px-3 py-3 rounded-[12px] transition-colors tool-list-item ${active ? "is-active" : ""}`}
    >
      <div className="flex items-start justify-between gap-3">
        <div className="min-w-0">
          {eyebrow ? (
            <div className="text-[10px] mb-1 uppercase tracking-[0.12em] text-[var(--text-quinary)]">{eyebrow}</div>
          ) : null}
          <div className="text-[13px] font-medium truncate text-[var(--text-secondary)]">{title}</div>
          {subtitle ? (
            <div className="text-[11px] mt-1 leading-4 truncate text-[var(--text-quaternary)]">{subtitle}</div>
          ) : null}
        </div>
        {trailing}
      </div>
    </button>
  );
}

export function ToolContentHeader({
  title,
  subtitle,
  badges
}: {
  title: string;
  subtitle?: string;
  badges?: ReactNode;
}) {
  return (
    <header
      className="px-7 py-6 border-b-[0.5px] border-b-[var(--panel-border)] tool-content-header"
    >
      <div className="flex flex-wrap items-start justify-between gap-3">
        <div className="min-w-0">
          <h1 className="text-[18px] font-semibold tracking-[-0.02em] truncate text-[var(--text-primary)]">{title}</h1>
          {subtitle ? (
            <p className="text-[12px] mt-1 leading-5 text-[var(--text-quaternary)]">{subtitle}</p>
          ) : null}
        </div>
        {badges ? <div className="flex flex-wrap items-center gap-2">{badges}</div> : null}
      </div>
    </header>
  );
}

export function ToolMetric({
  label,
  value
}: {
  label: string;
  value: string;
}) {
  return (
    <div className="rounded-[14px] px-4 py-3 tool-card">
      <div className="text-[11px] uppercase tracking-wider text-[var(--text-quaternary)]">{label}</div>
      <div className="text-[15px] mt-1 font-semibold tracking-[-0.01em] text-[var(--text-secondary)]">{value}</div>
    </div>
  );
}

export function ToolBadge({ label }: { label: string }) {
  return (
    <span className="px-2.5 py-1 rounded-full text-[11px] tool-badge">
      {label}
    </span>
  );
}

export function ToolMetaGrid({
  items
}: {
  items: Array<{ label: string; value: string }>;
}) {
  return (
    <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
      {items.map((item) => (
        <div
          key={item.label}
          className="rounded-[14px] px-4 py-3 tool-card"
        >
          <div className="text-[11px] uppercase tracking-wider text-[var(--text-quaternary)]">{item.label}</div>
          <div className="text-[13px] mt-1 break-all text-[var(--text-secondary)]">{item.value}</div>
        </div>
      ))}
    </div>
  );
}

export function ToolEmptyState({
  title,
  description
}: {
  title: string;
  description: string;
}) {
  return (
    <div className="flex h-full min-h-[320px] items-center justify-center px-6">
      <div className="text-center max-w-md">
        <div className="mx-auto mb-4 h-10 w-10 rounded-2xl tool-empty-glyph" />
        <div className="text-[15px] font-semibold text-[var(--text-tertiary)]">{title}</div>
        <p className="text-[12px] mt-2 leading-6 text-[var(--text-quaternary)]">{description}</p>
      </div>
    </div>
  );
}

export function ToolErrorBanner({ message }: { message: string }) {
  return (
    <div className="rounded-[14px] px-4 py-3 text-[13px] bg-[rgba(255,69,58,0.08)] border-[0.5px] border-[rgba(255,69,58,0.16)] text-[#ff9a91]">
      {message}
    </div>
  );
}

export function ToolActionButton({
  children,
  onClick,
  disabled,
  tone = "default"
}: {
  children: ReactNode;
  onClick: () => void;
  disabled?: boolean;
  tone?: "default" | "primary";
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      disabled={disabled}
      className={`px-3 py-2 rounded-[10px] text-[12px] font-medium transition-colors disabled:opacity-50 disabled:cursor-default tool-action ${tone === "primary" ? "is-primary" : ""}`}
    >
      {children}
    </button>
  );
}

export function formatBytes(value: number) {
  if (!Number.isFinite(value) || value <= 0) {
    return "0 B";
  }

  const units = ["B", "KB", "MB", "GB"];
  let size = value;
  let unitIndex = 0;
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex += 1;
  }
  return `${size.toFixed(size >= 10 || unitIndex === 0 ? 0 : 1)} ${units[unitIndex]}`;
}

export function formatNsTimestamp(value: number) {
  if (!Number.isFinite(value) || value <= 0) {
    return "未记录";
  }
  return new Date(value / 1_000_000).toLocaleString("zh-CN");
}

export function formatMsTimestamp(value: number) {
  if (!Number.isFinite(value) || value <= 0) {
    return "未记录";
  }
  return new Date(value).toLocaleString("zh-CN");
}

export function joinPath(directory: string, fileName: string) {
  const separator = directory.includes("\\") ? "\\" : "/";
  return `${directory.replace(/[\\/]+$/, "")}${separator}${fileName}`;
}
