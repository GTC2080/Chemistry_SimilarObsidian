import { useEffect, useState } from "react";

interface TextInputDialogProps {
  title: string;
  message?: string;
  defaultValue?: string;
  confirmLabel?: string;
  onCancel: () => void;
  onSubmit: (value: string) => void;
}

export default function TextInputDialog({
  title,
  message,
  defaultValue = "",
  confirmLabel = "确定",
  onCancel,
  onSubmit
}: TextInputDialogProps) {
  const [value, setValue] = useState(defaultValue);

  useEffect(() => {
    setValue(defaultValue);
  }, [defaultValue]);

  function submit() {
    const trimmed = value.trim();
    if (!trimmed) {
      return;
    }

    onSubmit(trimmed);
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/45">
      <div className="w-[360px] rounded-[18px] border-[0.5px] border-[var(--panel-border)] bg-[var(--panel-bg)] shadow-[0_24px_80px_rgba(0,0,0,0.45)]">
        <div className="px-5 pt-5 pb-3">
          <div className="text-[15px] font-semibold text-[var(--text-primary)]">{title}</div>
          {message ? (
            <div className="mt-2 text-[12px] leading-5 text-[var(--text-quaternary)]">{message}</div>
          ) : null}
          <input
            autoFocus
            value={value}
            onChange={(event) => setValue(event.target.value)}
            onKeyDown={(event) => {
              if (event.key === "Enter") {
                event.preventDefault();
                submit();
              } else if (event.key === "Escape") {
                event.preventDefault();
                onCancel();
              }
            }}
            className="mt-4 w-full h-10 rounded-[12px] px-3 text-[13px] outline-none border-[0.5px] border-[var(--panel-border)] bg-[var(--subtle-surface)] text-[var(--text-primary)]"
          />
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
            onClick={submit}
            disabled={!value.trim()}
            className="px-3 h-8 rounded-[10px] text-[12px] font-medium text-white bg-[var(--accent)] disabled:opacity-45 disabled:cursor-default transition-opacity"
          >
            {confirmLabel}
          </button>
        </div>
      </div>
    </div>
  );
}
