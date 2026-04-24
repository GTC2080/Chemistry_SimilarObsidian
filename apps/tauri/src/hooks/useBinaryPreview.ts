import { useCallback, useState } from "react";
import { convertFileSrc } from "@tauri-apps/api/core";
import type { NoteInfo } from "../types";

/**
 * Provides a preview URL for binary files (images) using Tauri's asset
 * protocol — zero IPC transfer, the WebView loads the file directly.
 */
export function useBinaryPreview() {
  const [binaryPreviewUrl, setBinaryPreviewUrl] = useState("");

  const clearBinaryPreview = useCallback(() => {
    setBinaryPreviewUrl("");
  }, []);

  const loadBinaryPreview = useCallback(async (note: NoteInfo) => {
    const url = convertFileSrc(note.path);
    setBinaryPreviewUrl(url);
    return url;
  }, []);

  return {
    binaryPreviewUrl,
    clearBinaryPreview,
    loadBinaryPreview,
  };
}
