export default function ToolButton({ onClick, icon, label, shortcut }: {
  onClick: () => void; icon: React.ReactNode; label: string; shortcut: string;
}) {
  return (
    <button
      onClick={onClick}
      className="w-full px-3 py-[7px] rounded-[10px] text-[13px]
        transition-colors duration-150 cursor-pointer flex items-center gap-2.5
        hover:bg-[var(--sidebar-hover)] active:bg-[var(--accent-soft)]"
      style={{ color: "var(--text-tertiary)" }}
    >
      {icon}
      <span className="flex-1 text-left">{label}</span>
      <kbd className="text-[10px] font-mono" style={{ color: "var(--text-quinary)" }}>{shortcut}</kbd>
    </button>
  );
}
