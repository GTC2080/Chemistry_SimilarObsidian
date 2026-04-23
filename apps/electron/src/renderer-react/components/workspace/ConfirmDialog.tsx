interface ConfirmDialogProps {
  title: string;
  message?: string;
  confirmLabel?: string;
  danger?: boolean;
  onCancel: () => void;
  onConfirm: () => void;
}

export default function ConfirmDialog({
  title,
  message,
  confirmLabel = "确定",
  danger = false,
  onCancel,
  onConfirm
}: ConfirmDialogProps) {
  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/45">
      <div className="w-[380px] rounded-[18px] border-[0.5px] border-[var(--panel-border)] bg-[var(--panel-bg)] shadow-[0_24px_80px_rgba(0,0,0,0.45)]">
        <div className="px-5 pt-5 pb-4">
          <div className="text-[15px] font-semibold text-[var(--text-primary)]">{title}</div>
          {message ? (
            <div className="mt-2 text-[12px] leading-5 text-[var(--text-quaternary)]">{message}</div>
          ) : null}
        </div>
        <div className="flex items-center justify-end gap-2 px-5 py-4 border-t-[0.5px] border-t-[var(--panel-border)]">
          <button
            type="button"
            onClick={onCancel}
            className="px-3 h-8 rounded-[10px] text-[12px] font-medium text-[var(--text-secondary)] bg-[var(--subtle-surface-strong)] hover:bg-[var(--sidebar-hover)] transition-colors"
          >
            取消
          </button>
          <button
            type="button"
            onClick={onConfirm}
            className="px-3 h-8 rounded-[10px] text-[12px] font-medium text-white transition-opacity"
            style={{ background: danger ? "#ff453a" : "var(--accent)" }}
          >
            {confirmLabel}
          </button>
        </div>
      </div>
    </div>
  );
}
