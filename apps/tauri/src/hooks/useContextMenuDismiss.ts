import { useEffect, type RefObject } from "react";

/**
 * Dismisses a context menu when the user clicks outside, presses Escape,
 * scrolls, or resizes the window. Shared by Sidebar and VaultManagerView.
 */
export function useContextMenuDismiss(
  isOpen: boolean,
  menuRef: RefObject<HTMLElement | null>,
  onClose: () => void,
) {
  useEffect(() => {
    if (!isOpen) return;

    const dismiss = () => onClose();

    const handlePointerDown = (e: Event) => {
      if (!menuRef.current?.contains(e.target as Node)) dismiss();
    };
    const handleContextMenu = (e: Event) => {
      if (!menuRef.current?.contains(e.target as Node)) dismiss();
    };
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") dismiss();
    };

    document.addEventListener("pointerdown", handlePointerDown, true);
    document.addEventListener("contextmenu", handleContextMenu, true);
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("wheel", dismiss, { passive: true });
    window.addEventListener("resize", dismiss);

    return () => {
      document.removeEventListener("pointerdown", handlePointerDown, true);
      document.removeEventListener("contextmenu", handleContextMenu, true);
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("wheel", dismiss);
      window.removeEventListener("resize", dismiss);
    };
  }, [isOpen, menuRef, onClose]);
}
