import { useEffect, useState } from "react";

interface PdfViewerProps {
  pdfBytes: Uint8Array | null;
  placeholder?: string;
}

export default function PdfViewer({
  pdfBytes,
  placeholder = "Build PDF to preview",
}: PdfViewerProps) {
  const [blobUrl, setBlobUrl] = useState("");

  useEffect(() => {
    if (!pdfBytes || pdfBytes.length === 0) {
      setBlobUrl("");
      return;
    }

    const bytes = new Uint8Array(pdfBytes.length);
    bytes.set(pdfBytes);
    const blob = new Blob([bytes.buffer], { type: "application/pdf" });
    const url = URL.createObjectURL(blob);
    setBlobUrl(url);

    return () => {
      URL.revokeObjectURL(url);
    };
  }, [pdfBytes]);

  if (!blobUrl) {
    return (
      <div className="h-full w-full rounded-md bg-[#0D0D0D] shadow-[inset_0_0_0_1px_rgba(255,255,255,0.06)] flex items-center justify-center text-sm text-[#888]">
        {placeholder}
      </div>
    );
  }

  return (
    <div className="h-full w-full rounded-md overflow-hidden bg-[#0B0B0B] shadow-[inset_0_0_0_1px_rgba(255,255,255,0.06),0_18px_42px_rgba(0,0,0,0.55)]">
      <iframe src={blobUrl} className="w-full h-full border-none shadow-2xl" title="PDF Preview" />
    </div>
  );
}
