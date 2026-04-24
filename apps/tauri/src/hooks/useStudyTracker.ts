import { useCallback, useEffect, useRef } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo } from "../types";

const TICK_INTERVAL_MS = 30_000;
const IDLE_TIMEOUT_MS = 5 * 60_000;
const ACTIVITY_EVENTS = ["keydown", "mousemove", "mousedown", "scroll"] as const;

export function useStudyTracker(activeNote: NoteInfo | null, vaultPath: string): void {
  const sessionIdRef = useRef<number | null>(null);
  const activeSecsRef = useRef<number>(0);
  const lastActivityRef = useRef<number>(Date.now());
  const tickTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const activeNoteIdRef = useRef<string | null>(null);
  const rafHandleRef = useRef<number | null>(null);
  const lastSecondRef = useRef<number>(Date.now());

  // Fire-and-forget session end (used in beforeunload where we can't await)
  const fireEndSession = useCallback((sid: number, secs: number) => {
    invoke("study_session_end", { sessionId: sid, activeSecs: secs })
      .then(() => window.dispatchEvent(new Event("study-tick")))
      .catch(e => console.warn("study_session_end failed:", e));
  }, []);

  const stopTickTimer = useCallback(() => {
    if (tickTimerRef.current !== null) {
      clearTimeout(tickTimerRef.current);
      tickTimerRef.current = null;
    }
  }, []);

  const scheduleTickTimer = useCallback((sid: number) => {
    stopTickTimer();
    tickTimerRef.current = setTimeout(() => {
      const secs = activeSecsRef.current;
      activeSecsRef.current = 0;
      invoke("study_session_tick", { sessionId: sid, activeSecs: secs })
        .then(() => window.dispatchEvent(new Event("study-tick")))
        .catch(e => console.warn("study_session_tick failed:", e));
      scheduleTickTimer(sid);
    }, TICK_INTERVAL_MS);
  }, [stopTickTimer]);

  const endSession = useCallback(() => {
    stopTickTimer();
    const sid = sessionIdRef.current;
    if (sid !== null) {
      const secs = activeSecsRef.current;
      activeSecsRef.current = 0;
      sessionIdRef.current = null;
      fireEndSession(sid, secs);
    }
  }, [stopTickTimer, fireEndSession]);

  const startSession = useCallback(async (note: NoteInfo) => {
    const noteId = note.id.replace(/\\/g, "/");
    const folder = noteId.includes("/") ? noteId.substring(0, noteId.lastIndexOf("/")) : "";
    try {
      const sid = await invoke<number>("study_session_start", { noteId, folder });
      sessionIdRef.current = sid;
      activeSecsRef.current = 0;
      scheduleTickTimer(sid);
    } catch {
      // Silently ignore — tracking is best-effort
    }
  }, [scheduleTickTimer]);

  // RAF loop: increment activeSecsRef once per second when not idle
  useEffect(() => {
    let running = true;

    const loop = () => {
      if (!running) return;
      const now = Date.now();
      const elapsed = now - lastSecondRef.current;
      if (elapsed >= 1000) {
        lastSecondRef.current = now;
        if (now - lastActivityRef.current < IDLE_TIMEOUT_MS) {
          activeSecsRef.current += Math.floor(elapsed / 1000);
        }
      }
      rafHandleRef.current = requestAnimationFrame(loop);
    };

    rafHandleRef.current = requestAnimationFrame(loop);

    return () => {
      running = false;
      if (rafHandleRef.current !== null) {
        cancelAnimationFrame(rafHandleRef.current);
        rafHandleRef.current = null;
      }
    };
  }, []);

  // Activity event listeners
  useEffect(() => {
    const onActivity = () => {
      lastActivityRef.current = Date.now();
    };

    for (const event of ACTIVITY_EVENTS) {
      window.addEventListener(event, onActivity, { passive: true });
    }

    return () => {
      for (const event of ACTIVITY_EVENTS) {
        window.removeEventListener(event, onActivity);
      }
    };
  }, []);

  // Session lifecycle: react to activeNote changes
  useEffect(() => {
    const newNoteId = activeNote ? activeNote.id.replace(/\\/g, "/") : null;

    if (newNoteId === activeNoteIdRef.current) return;

    // End any previous session
    endSession();
    activeNoteIdRef.current = newNoteId;

    if (activeNote !== null) {
      void startSession(activeNote);
    }
  }, [activeNote, endSession, startSession]);

  // beforeunload: fire-and-forget end
  useEffect(() => {
    const onBeforeUnload = () => {
      stopTickTimer();
      const sid = sessionIdRef.current;
      if (sid !== null) {
        const secs = activeSecsRef.current;
        activeSecsRef.current = 0;
        sessionIdRef.current = null;
        fireEndSession(sid, secs);
      }
    };

    window.addEventListener("beforeunload", onBeforeUnload);
    return () => window.removeEventListener("beforeunload", onBeforeUnload);
  }, [stopTickTimer, fireEndSession]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      endSession();
    };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Suppress unused-variable warning for vaultPath (available for future use)
  void vaultPath;
}
