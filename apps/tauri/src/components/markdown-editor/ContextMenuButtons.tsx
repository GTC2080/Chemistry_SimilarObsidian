export function ContextMenuButton({
  label,
  onClick,
  disabled = false,
}: {
  label: string;
  onClick: () => void;
  disabled?: boolean;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      disabled={disabled}
      className="w-full rounded px-2 py-1.5 text-left text-[12px] transition-colors disabled:opacity-40 disabled:cursor-not-allowed hover:bg-[#1F1F1F]"
      style={{ color: "#D8D8D8" }}
    >
      {label}
    </button>
  );
}

export function ContextIconButton({
  label,
  title,
  onClick,
  disabled = false,
  children,
}: {
  label: string;
  title: string;
  onClick: () => void;
  disabled?: boolean;
  children: React.ReactNode;
}) {
  return (
    <button
      type="button"
      title={title}
      onClick={onClick}
      disabled={disabled}
      className="flex flex-col items-center justify-center gap-1 rounded-lg px-2 py-2 text-[11px] transition-colors disabled:opacity-40 disabled:cursor-not-allowed hover:bg-[#1A1A1A]"
      style={{ color: "#D8D8D8" }}
    >
      {children}
      <span className="leading-none">{label}</span>
    </button>
  );
}

export function ContextFormatButton({
  label,
  onClick,
  active = false,
  italic = false,
  mono = false,
}: {
  label: string;
  onClick: () => void;
  active?: boolean;
  italic?: boolean;
  mono?: boolean;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="h-8 rounded-lg border text-[12px] transition-colors"
      style={{
        color: active ? "#EDEDED" : "#B4B4B4",
        borderColor: active ? "#3B82F6" : "#2A2A2A",
        background: active ? "rgba(59,130,246,0.18)" : "#151515",
        fontStyle: italic ? "italic" : undefined,
        fontFamily: mono ? '"SF Mono", "Fira Code", Consolas, monospace' : undefined,
      }}
    >
      {label}
    </button>
  );
}

export function ContextSubmenuButton({
  label,
  active,
  onHover,
}: {
  label: string;
  active: boolean;
  onHover: () => void;
}) {
  return (
    <button
      type="button"
      onMouseEnter={onHover}
      onClick={onHover}
      className="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-[12px] transition-colors"
      style={{
        color: "#D8D8D8",
        background: active ? "#1F1F1F" : "transparent",
      }}
    >
      <span>{label}</span>
      <span className="text-[#6F6F6F]">›</span>
    </button>
  );
}
