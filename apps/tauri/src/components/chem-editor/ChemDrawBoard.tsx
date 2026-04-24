import { useEffect, useRef, useCallback } from "react";
import { Editor } from "ketcher-react";
import { StandaloneStructServiceProvider } from "ketcher-standalone";
import "ketcher-react/dist/index.css";
import { useLanguage } from "../../i18n";
import { startKetcherLocale } from "./ketcherLocale";

interface ChemDrawBoardProps {
  initialContent: string;
  onSave: (content: string) => void | Promise<void>;
}

const structServiceProvider = new StandaloneStructServiceProvider();

export default function ChemDrawBoard({ initialContent, onSave }: ChemDrawBoardProps) {
  const ketcherRef = useRef<any>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const lang = useLanguage();

  const handleInit = useCallback((ketcher: any) => {
    ketcherRef.current = ketcher;
    if (initialContent.trim()) {
      ketcher.setMolecule(initialContent);
    }
  }, [initialContent]);

  useEffect(() => {
    const handler = async (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key === "s") {
        e.preventDefault();
        if (!ketcherRef.current) return;
        const molfile = await ketcherRef.current.getMolfile();
        onSave(molfile);
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [onSave]);

  useEffect(() => {
    return () => { ketcherRef.current = null; };
  }, []);

  // Ketcher 汉化
  useEffect(() => {
    if (lang !== "zh-CN" || !containerRef.current) return;
    return startKetcherLocale(containerRef.current);
  }, [lang]);

  return (
    <div ref={containerRef} className="flex-1 h-full w-full bg-[#050505] chemdraw-container">
      <Editor
        staticResourcesUrl=""
        structServiceProvider={structServiceProvider}
        onInit={handleInit}
        errorHandler={(message: string) => console.error("[ChemDraw]", message)}
      />
    </div>
  );
}
