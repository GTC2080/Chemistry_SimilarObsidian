import { useState, useRef, useCallback, useEffect } from "react";

interface UseInlineRenameOptions {
  fullName: string;
  relativePath: string;
  onInlineRename: (path: string, newName: string) => void;
}

export function useInlineRename({
  fullName,
  relativePath,
  onInlineRename,
}: UseInlineRenameOptions) {
  const [renaming, setRenaming] = useState(false);
  const [renameValue, setRenameValue] = useState(fullName);
  const renameInputRef = useRef<HTMLInputElement | null>(null);

  useEffect(() => {
    if (!renaming) return;
    const timer = window.setTimeout(() => {
      if (!renameInputRef.current) return;
      renameInputRef.current.focus();
      renameInputRef.current.select();
    }, 0);
    return () => window.clearTimeout(timer);
  }, [renaming]);

  useEffect(() => {
    setRenameValue(fullName);
    setRenaming(false);
  }, [fullName]);

  const beginRename = useCallback(() => {
    setRenameValue(fullName);
    setRenaming(true);
  }, [fullName]);

  const commitRename = useCallback(() => {
    const next = renameValue.trim();
    setRenaming(false);
    if (!next || next === fullName) return;
    onInlineRename(relativePath, next);
  }, [renameValue, fullName, relativePath, onInlineRename]);

  const cancelRename = useCallback(() => {
    setRenaming(false);
    setRenameValue(fullName);
  }, [fullName]);

  return {
    renaming,
    renameValue,
    renameInputRef,
    setRenameValue,
    beginRename,
    commitRename,
    cancelRename,
  };
}
