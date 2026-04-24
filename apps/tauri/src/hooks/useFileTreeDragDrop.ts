import { useState, useRef, useCallback } from "react";
import type { DragEvent } from "react";

interface UseFileTreeDragDropOptions {
  relativePath: string;
  isFolder: boolean;
  renaming: boolean;
  onMoveToFolder: (src: string, dest: string) => void;
}

export function useFileTreeDragDrop({
  relativePath,
  isFolder,
  renaming,
  onMoveToFolder,
}: UseFileTreeDragDropOptions) {
  const [dragOver, setDragOver] = useState(false);
  const dragCountRef = useRef(0);

  const handleDragStart = useCallback((e: DragEvent) => {
    if (renaming) {
      e.preventDefault();
      return;
    }
    e.dataTransfer.setData("text/x-filetree-path", relativePath);
    e.dataTransfer.setData("text/x-filetree-isfolder", isFolder ? "1" : "0");
    e.dataTransfer.effectAllowed = "move";
    // Make dragged item semi-transparent
    if (e.currentTarget instanceof HTMLElement) {
      e.currentTarget.style.opacity = "0.4";
    }
  }, [relativePath, isFolder, renaming]);

  const handleDragEnd = useCallback((e: DragEvent) => {
    if (e.currentTarget instanceof HTMLElement) {
      e.currentTarget.style.opacity = "";
    }
  }, []);

  const handleDragOver = useCallback((e: DragEvent) => {
    if (!isFolder) return;
    e.preventDefault();
    e.stopPropagation();
    e.dataTransfer.dropEffect = "move";
  }, [isFolder]);

  const handleDragEnter = useCallback((e: DragEvent) => {
    if (!isFolder) return;
    e.preventDefault();
    e.stopPropagation();
    dragCountRef.current++;
    if (dragCountRef.current === 1) {
      setDragOver(true);
    }
  }, [isFolder]);

  const handleDragLeave = useCallback((e: DragEvent) => {
    if (!isFolder) return;
    e.stopPropagation();
    dragCountRef.current--;
    if (dragCountRef.current <= 0) {
      dragCountRef.current = 0;
      setDragOver(false);
    }
  }, [isFolder]);

  const handleDrop = useCallback((e: DragEvent) => {
    if (!isFolder) return;
    e.preventDefault();
    e.stopPropagation();
    dragCountRef.current = 0;
    setDragOver(false);

    const sourcePath = e.dataTransfer.getData("text/x-filetree-path");
    if (!sourcePath || sourcePath === relativePath) return;

    // Don't drop into its current parent (already there)
    const sourceParent = sourcePath.includes("/")
      ? sourcePath.substring(0, sourcePath.lastIndexOf("/"))
      : "";
    if (sourceParent === relativePath) return;

    // Don't drop a folder into its own subtree
    if (relativePath.startsWith(sourcePath + "/")) return;

    onMoveToFolder(sourcePath, relativePath);
  }, [relativePath, isFolder, onMoveToFolder]);

  return {
    dragOver,
    handleDragStart,
    handleDragEnd,
    handleDragOver,
    handleDragEnter,
    handleDragLeave,
    handleDrop,
  };
}
