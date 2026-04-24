import { useEffect, useState } from "react";
import { getVersion } from "@tauri-apps/api/app";

export function useAppVersion(): string {
  const [version, setVersion] = useState("...");

  useEffect(() => {
    let cancelled = false;

    void getVersion()
      .then(v => {
        if (!cancelled) {
          setVersion(v);
        }
      })
      .catch(() => {
        if (!cancelled) {
          setVersion("unknown");
        }
      });

    return () => {
      cancelled = true;
    };
  }, []);

  return version;
}
