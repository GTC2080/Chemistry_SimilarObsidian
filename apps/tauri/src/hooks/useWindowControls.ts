import { useMemo } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";

export function useWindowControls() {
  const appWindow = useMemo(() => getCurrentWindow(), []);

  return {
    onMinimize: () => appWindow.minimize(),
    onToggleMaximize: () => appWindow.toggleMaximize(),
    onClose: () => appWindow.close(),
    onBackgroundMouseDown: (e: React.MouseEvent<HTMLDivElement>) => {
      if (!(e.target as HTMLElement).closest("button")) {
        void appWindow.startDragging();
      }
    },
    onBackgroundDoubleClick: (e: React.MouseEvent<HTMLDivElement>) => {
      if (!(e.target as HTMLElement).closest("button")) {
        void appWindow.toggleMaximize();
      }
    },
  };
}
