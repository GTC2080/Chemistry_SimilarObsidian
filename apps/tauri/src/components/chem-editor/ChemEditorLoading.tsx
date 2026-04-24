import { useT } from "../../i18n";

/**
 * Ketcher 化学编辑器加载占位组件。
 * 在 25MB 的 Ketcher chunk 懒加载期间显示，
 * 给用户即时反馈而非空白等待。
 */
export default function ChemEditorLoading() {
  const t = useT();
  return (
    <div className="flex-1 h-full w-full flex flex-col items-center justify-center gap-4 bg-[#050505] select-none">
      {/* Molecule icon */}
      <svg
        className="w-12 h-12 chem-loading-icon"
        viewBox="0 0 24 24"
        fill="none"
        stroke="rgba(59,130,246,0.5)"
        strokeWidth="1.2"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        <circle cx="12" cy="12" r="3" />
        <circle cx="5" cy="6" r="2" />
        <circle cx="19" cy="6" r="2" />
        <circle cx="5" cy="18" r="2" />
        <circle cx="19" cy="18" r="2" />
        <line x1="9.5" y1="10" x2="6.5" y2="7.5" />
        <line x1="14.5" y1="10" x2="17.5" y2="7.5" />
        <line x1="9.5" y1="14" x2="6.5" y2="16.5" />
        <line x1="14.5" y1="14" x2="17.5" y2="16.5" />
      </svg>

      {/* Loading bar */}
      <div className="w-20 h-[2px] rounded-full overflow-hidden bg-[rgba(255,255,255,0.06)]">
        <div className="h-full w-full launch-bar" />
      </div>

      <span className="text-[12px] text-[rgba(255,255,255,0.25)]">
        {t("molecule.loading")}
      </span>
    </div>
  );
}
