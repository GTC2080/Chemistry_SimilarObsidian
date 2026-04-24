import { useEffect, useState } from "react";

export function useLazyModalReady(open: boolean): boolean {
  const [ready, setReady] = useState(false);

  useEffect(() => {
    if (open) setReady(true);
  }, [open]);

  return ready;
}
