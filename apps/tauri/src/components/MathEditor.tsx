import { useState, useRef, useEffect, useCallback } from "react";
import katex from "katex";

interface MathEditorProps {
  /** 当前 LaTeX 源码 */
  latex: string;
  /** 是否为块级公式 */
  isBlock: boolean;
  /** 弹出位置（相对于视口） */
  anchorRect: DOMRect | null;
  /** 确认编辑 */
  onConfirm: (newLatex: string) => void;
  /** 取消/关闭 */
  onClose: () => void;
}

/**
 * 公式编辑浮层：点击公式后弹出，左侧输入 LaTeX，右侧实时预览渲染结果。
 * Enter 确认，Escape 取消。
 */
export default function MathEditor({ latex, isBlock, anchorRect, onConfirm, onClose }: MathEditorProps) {
  const [value, setValue] = useState(latex);
  const [error, setError] = useState("");
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const previewRef = useRef<HTMLDivElement>(null);
  const panelRef = useRef<HTMLDivElement>(null);

  // 聚焦输入框
  useEffect(() => {
    requestAnimationFrame(() => {
      inputRef.current?.focus();
      inputRef.current?.select();
    });
  }, []);

  // 实时渲染预览
  useEffect(() => {
    if (!previewRef.current) return;
    try {
      katex.render(value, previewRef.current, {
        throwOnError: true,
        displayMode: isBlock,
      });
      setError("");
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, [value, isBlock]);

  const handleConfirm = useCallback(() => {
    const trimmed = value.trim();
    if (trimmed) {
      onConfirm(trimmed);
    }
    onClose();
  }, [value, onConfirm, onClose]);

  function handleKeyDown(e: React.KeyboardEvent) {
    if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) {
      e.preventDefault();
      handleConfirm();
    }
    if (e.key === "Escape") {
      e.preventDefault();
      onClose();
    }
  }

  // 计算浮层位置：优先公式下方，空间不足则翻转到上方
  const style: React.CSSProperties = {};
  if (anchorRect) {
    const panelWidth = 480;
    const panelMaxHeight = 320;
    const gap = 8;
    let left = anchorRect.left + anchorRect.width / 2 - panelWidth / 2;
    left = Math.max(gap, Math.min(left, window.innerWidth - panelWidth - gap));

    const spaceBelow = window.innerHeight - anchorRect.bottom - gap;
    const spaceAbove = anchorRect.top - gap;
    const fitsBelow = spaceBelow >= panelMaxHeight;

    let top: number;
    let maxH: number;
    if (fitsBelow) {
      top = anchorRect.bottom + gap;
      maxH = panelMaxHeight;
    } else if (spaceAbove > spaceBelow) {
      // 翻转到上方
      maxH = Math.min(panelMaxHeight, spaceAbove);
      top = anchorRect.top - gap - maxH;
    } else {
      top = anchorRect.bottom + gap;
      maxH = Math.max(160, spaceBelow);
    }

    style.position = "fixed";
    style.left = `${left}px`;
    style.top = `${Math.max(gap, top)}px`;
    style.width = `${panelWidth}px`;
    style.maxHeight = `${maxH}px`;
    style.zIndex = 100;
  }

  return (
    <>
      {/* 背景遮罩（透明，仅用于捕获点击关闭） */}
      <div className="fixed inset-0 z-[99]" onClick={onClose} />

      {/* 编辑面板 */}
      <div
        ref={panelRef}
        className="animate-fade-in glass-elevated glass-highlight rounded-[16px] overflow-hidden flex flex-col"
        style={{
          ...style,
          background: "var(--glass-bg-elevated)",
          overflowY: "auto",
        }}
      >
        {/* 标题 */}
        <div className="px-4 py-2.5 flex items-center justify-between"
          style={{ borderBottom: "0.5px solid var(--panel-border)" }}>
          <span className="text-[12px] font-medium" style={{ color: "var(--text-tertiary)" }}>
            {isBlock ? "块级公式" : "行内公式"}
          </span>
          <div className="flex items-center gap-2">
            <span className="text-[10px]" style={{ color: "var(--text-quaternary)" }}>
              <kbd className="px-1 py-[1px] rounded text-[9px] font-mono"
                style={{ background: "var(--subtle-surface-strong)" }}>Ctrl+Enter</kbd> 确认
            </span>
            <span className="text-[10px]" style={{ color: "var(--text-quaternary)" }}>
              <kbd className="px-1 py-[1px] rounded text-[9px] font-mono"
                style={{ background: "var(--subtle-surface-strong)" }}>Esc</kbd> 取消
            </span>
          </div>
        </div>

        {/* LaTeX 输入框 */}
        <div className="px-3 pt-3 pb-2">
          <textarea
            ref={inputRef}
            value={value}
            onChange={e => setValue(e.target.value)}
            onKeyDown={handleKeyDown}
            rows={isBlock ? 3 : 1}
            className="w-full bg-transparent text-[13px] leading-relaxed outline-none resize-none rounded-[10px] px-3 py-2"
            style={{
              color: "var(--text-primary)",
              caretColor: "var(--accent)",
              background: "var(--subtle-surface)",
              border: "0.5px solid var(--separator-light)",
              fontFamily: '"SF Mono", "Fira Code", "Cascadia Code", Consolas, monospace',
            }}
            placeholder="输入 LaTeX 公式…"
          />
        </div>

        {/* 实时预览 */}
        <div className="px-3 pb-3">
          <div className="text-[10px] mb-1.5 px-1" style={{ color: "var(--text-quaternary)" }}>
            预览
          </div>
          {error ? (
            <div className="text-[11px] px-3 py-2 rounded-[8px]"
              style={{ color: "#ff453a", background: "rgba(255,69,58,0.08)" }}>
              {error}
            </div>
          ) : (
            <div
              ref={previewRef}
              className="px-3 py-2 rounded-[10px] overflow-x-auto"
              style={{
                background: "var(--subtle-surface)",
                border: "0.5px solid var(--separator-light)",
                textAlign: isBlock ? "center" : "left",
                color: "var(--text-primary)",
              }}
            />
          )}
        </div>

        {/* 底部操作 */}
        <div className="px-3 pb-3 flex justify-end gap-2">
          <button
            type="button"
            onClick={onClose}
            className="px-3 py-1.5 rounded-[8px] text-[12px] cursor-pointer transition-all duration-150
              hover:bg-[var(--sidebar-hover)]"
            style={{ color: "var(--text-tertiary)" }}
          >
            取消
          </button>
          <button
            type="button"
            onClick={handleConfirm}
            className="apple-btn px-4 py-1.5 rounded-[8px] text-[12px] font-medium cursor-pointer"
            style={{
              background: "linear-gradient(135deg, #0A84FF 0%, #0070E0 100%)",
              color: "#fff",
              boxShadow: "0 2px 6px rgba(10,132,255,0.3)",
            }}
          >
            确认
          </button>
        </div>
      </div>
    </>
  );
}
