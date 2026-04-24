import { useState } from "react";
import type { SettingsState } from "./settingsTypes";

export type SettingsUpdate = <K extends keyof SettingsState>(key: K, value: SettingsState[K]) => void;

export interface SelectOption<T extends string> {
  value: T;
  label: string;
}

export interface TestResult {
  ok: boolean;
  msg: string;
}

export const inputClass = "w-full rounded-[10px] px-3 py-2.5 text-sm transition-all placeholder:text-[var(--text-quaternary)] bg-[rgba(255,255,255,0.03)] border border-[var(--separator-light)] text-[var(--text-secondary)] focus:outline-none focus:border-[var(--accent)] focus:bg-[rgba(10,132,255,0.07)]";
export const labelClass = "block text-sm font-medium text-[var(--text-secondary)] mb-1.5";
export const hintClass = "text-xs text-[var(--text-quaternary)] mt-1";

export function ThemedSelect<T extends string>({
  value,
  options,
  onChange,
}: {
  value: T;
  options: Array<SelectOption<T>>;
  onChange: (next: T) => void;
}) {
  const [open, setOpen] = useState(false);
  const selected = options.find(option => option.value === value) ?? options[0];

  return (
    <div className="relative" tabIndex={0} onBlur={() => setOpen(false)}>
      <button
        type="button"
        className={inputClass + " w-full text-left pr-9 cursor-pointer"}
        onClick={() => setOpen(prev => !prev)}
        aria-haspopup="listbox"
        aria-expanded={open}
      >
        {selected?.label ?? ""}
        <span className="absolute right-3 top-1/2 -translate-y-1/2 pointer-events-none text-[var(--text-tertiary)]">
          <svg className={"w-4 h-4 transition-transform " + (open ? "rotate-180" : "")} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <polyline points="6 9 12 15 18 9" />
          </svg>
        </span>
      </button>

      {open && (
        <div
          className="absolute left-0 right-0 mt-1 z-30 rounded-[10px] border border-[var(--separator-light)] bg-[var(--surface-2)] shadow-[0_12px_28px_rgba(0,0,0,0.35)] overflow-hidden"
          role="listbox"
        >
          {options.map(option => (
            <button
              key={option.value}
              type="button"
              onMouseDown={e => e.preventDefault()}
              onClick={() => {
                onChange(option.value);
                setOpen(false);
              }}
              className={`w-full text-left px-3 py-2 text-sm transition-colors ${
                option.value === value
                  ? "bg-[var(--accent-soft)] text-[var(--text-primary)]"
                  : "text-[var(--text-secondary)] hover:bg-[rgba(255,255,255,0.06)]"
              }`}
            >
              {option.label}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
