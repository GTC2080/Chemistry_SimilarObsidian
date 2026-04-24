import { useRef, useCallback, useEffect } from "react";
import { Editor as KetcherEditor } from "ketcher-react";
import { StandaloneStructServiceProvider } from "ketcher-standalone";
import "ketcher-react/dist/index.css";
import { useT, useLanguage } from "../../i18n";
import { startKetcherLocale } from "./ketcherLocale";

interface ChemDrawModalProps {
  open: boolean;
  onClose: () => void;
  onConfirm: (smiles: string) => void;
}

const structServiceProvider = new StandaloneStructServiceProvider();

export default function ChemDrawModal({ open, onClose, onConfirm }: ChemDrawModalProps) {
  const t = useT();
  const lang = useLanguage();
  const ketcherRef = useRef<any>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  const handleInit = useCallback((ketcher: any) => {
    ketcherRef.current = ketcher;
  }, []);

  const handleConfirm = useCallback(async () => {
    if (!ketcherRef.current) return;
    const smiles = await ketcherRef.current.getSmiles();
    if (smiles && smiles.trim()) {
      onConfirm(smiles.trim());
    }
  }, [onConfirm]);

  useEffect(() => {
    if (!open) ketcherRef.current = null;
  }, [open]);

  // Ketcher 汉化
  useEffect(() => {
    if (!open || lang !== "zh-CN" || !containerRef.current) return;
    return startKetcherLocale(containerRef.current);
  }, [open, lang]);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-[200] flex items-center justify-center bg-black/60" onClick={onClose}>
      <div
        ref={containerRef}
        className="w-[80vw] h-[70vh] bg-[#0A0A0A] border border-[#222] rounded-xl flex flex-col overflow-hidden"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex-1 chemdraw-container">
          <KetcherEditor
            structServiceProvider={structServiceProvider}
            onInit={handleInit}
            staticResourcesUrl=""
            errorHandler={(msg: string) => console.error("[ChemDrawModal]", msg)}
          />
        </div>
        <div className="flex justify-end gap-3 px-4 py-3 border-t border-[#222] bg-[#0E0E0E]">
          <button
            type="button"
            className="px-4 py-1.5 text-sm rounded bg-[#1A1A1A] text-[#999] hover:text-[#EDEDED] border border-[#333] transition-colors"
            onClick={onClose}
          >
            {t("settings.cancel")}
          </button>
          <button
            type="button"
            className="px-4 py-1.5 text-sm rounded bg-[#3B82F6] text-white hover:bg-[#2563EB] transition-colors"
            onClick={handleConfirm}
          >
            {t("chemdraw.save")}
          </button>
        </div>
      </div>
    </div>
  );
}
