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
        className="w-[300px] shrink-0 border-r-[0.5px] border-r-[var(--panel-border)] overflow-y-auto workspace-panel tool-sidebar"
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
    <section className="px-4 py-3.5 border-b-[0.5px] border-b-[var(--panel-border)] tool-section">
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
      className={`w-full text-left px-3 py-2.5 rounded-[11px] transition-colors tool-list-item ${active ? "is-active" : ""}`}
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
      className="px-7 py-5 border-b-[0.5px] border-b-[var(--panel-border)] tool-content-header"
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
  value,
  hint
}: {
  label: string;
  value: string;
  hint?: string;
}) {
  return (
    <div className="rounded-[13px] px-4 py-3 tool-card">
      <div className="text-[11px] uppercase tracking-wider text-[var(--text-quaternary)]">{label}</div>
      <div className="text-[15px] mt-1 font-semibold tracking-[-0.01em] text-[var(--text-secondary)]">{value}</div>
      {hint ? (
        <div className="text-[11px] mt-1 truncate text-[var(--text-quaternary)]">{hint}</div>
      ) : null}
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
          className="rounded-[13px] px-4 py-3 tool-card"
        >
          <div className="text-[11px] uppercase tracking-wider text-[var(--text-quaternary)]">{item.label}</div>
          <div className="text-[13px] mt-1 break-all text-[var(--text-secondary)]">{item.value}</div>
        </div>
      ))}
    </div>
  );
}

export function ToolBody({ children }: { children: ReactNode }) {
  return (
    <div className="p-6 space-y-6 tool-body">
      {children}
    </div>
  );
}

export function ToolDetailSection({
  title,
  subtitle,
  children
}: {
  title: string;
  subtitle?: string;
  children: ReactNode;
}) {
  return (
    <section className="space-y-3">
      <div>
        <h2 className="text-[13px] font-semibold text-[var(--text-primary)]">{title}</h2>
        {subtitle ? (
          <p className="text-[11px] mt-1 leading-5 text-[var(--text-quaternary)]">{subtitle}</p>
        ) : null}
      </div>
      {children}
    </section>
  );
}

export function ToolReferenceCard({
  title,
  subtitle,
  meta,
  action,
  children
}: {
  title: string;
  subtitle?: string;
  meta?: string;
  action?: ReactNode;
  children?: ReactNode;
}) {
  return (
    <div className="rounded-[13px] px-4 py-3 tool-card">
      <div className="flex items-start justify-between gap-3">
        <div className="min-w-0">
          <div className="text-[13px] font-medium truncate text-[var(--text-secondary)]">{title}</div>
          {subtitle ? (
            <div className="text-[11px] mt-1 truncate text-[var(--text-quaternary)]">{subtitle}</div>
          ) : null}
        </div>
        {action}
      </div>
      {meta ? (
        <div className="text-[12px] mt-3 text-[var(--text-tertiary)]">{meta}</div>
      ) : null}
      {children ? (
        <div className="text-[11px] mt-2 break-all text-[var(--text-quaternary)]">{children}</div>
      ) : null}
    </div>
  );
}

export function ToolDevDetails({
  title = "开发详情",
  subtitle,
  children
}: {
  title?: string;
  subtitle?: string;
  children: ReactNode;
}) {
  return (
    <details className="rounded-[13px] px-4 py-3 tool-card group">
      <summary className="cursor-pointer list-none">
        <div className="flex items-center justify-between gap-3">
          <div>
            <div className="text-[12px] font-medium text-[var(--text-tertiary)]">{title}</div>
            {subtitle ? (
              <div className="text-[11px] mt-1 text-[var(--text-quaternary)]">{subtitle}</div>
            ) : null}
          </div>
          <span className="text-[13px] text-[var(--text-quinary)] group-open:rotate-90 transition-transform">›</span>
        </div>
      </summary>
      <div className="mt-4">
        {children}
      </div>
    </details>
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

export function formatCompactState(label: string, value: string | number | boolean | null | undefined) {
  return `${label} ${value == null || value === "" ? "none" : String(value)}`;
}

export function formatPresence(value: number) {
  if (value === 0) {
    return "present";
  }
  if (value === 1) {
    return "missing";
  }
  return `presence ${value}`;
}

export function formatPresenceLabel(value: number) {
  if (value === 0) {
    return "可用";
  }
  if (value === 1) {
    return "缺失";
  }
  return `状态 ${value}`;
}

export function formatAttachmentKind(value: number) {
  if (value === 3) {
    return "pdf-like";
  }
  if (value === 4) {
    return "chem-like";
  }
  return `kind ${value}`;
}

export function formatAttachmentKindLabel(value: number) {
  if (value === 3) {
    return "PDF";
  }
  if (value === 4) {
    return "谱图";
  }
  return "附件";
}

export function formatChemSourceFormat(value: number) {
  if (value === 1) {
    return "JCAMP-DX";
  }
  return `format ${value}`;
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
