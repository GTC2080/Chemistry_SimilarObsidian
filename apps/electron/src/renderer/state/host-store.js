/**
 * host-store.js
 *
 * Single source of truth for host envelope data.
 * ONLY caches host envelopes. No second business truth layer.
 */

import { classifyEnvelope } from "../services/envelope-guard.js";

const STORE_KEYS = [
  "bootstrap",
  "runtime",
  "session",
  "search",
  "attachments",
  "pdf",
  "chemistry",
  "diagnostics",
  "rebuild"
];

class HostStore {
  constructor() {
    this._state = Object.fromEntries(STORE_KEYS.map((k) => [k, null]));
    this._listeners = new Set();
    this._hostAvailable = false;
    this._currentPage = "launcher";
  }

  subscribe(listener) {
    this._listeners.add(listener);
    return () => this._listeners.delete(listener);
  }

  _notify() {
    this._listeners.forEach((fn) => {
      try {
        fn(this.snapshot());
      } catch (err) {
        // eslint-disable-next-line no-console
        console.error("HostStore listener error:", err);
      }
    });
  }

  /**
   * Store a raw envelope under a key.
   * @param {string} key
   * @param {any} envelope
   */
  setEnvelope(key, envelope) {
    if (!STORE_KEYS.includes(key)) {
      // eslint-disable-next-line no-console
      console.warn(`HostStore: ignoring unknown key '${key}'`);
      return;
    }
    this._state[key] = envelope;
    this._notify();
  }

  getEnvelope(key) {
    return this._state[key];
  }

  setHostAvailable(value) {
    this._hostAvailable = Boolean(value);
    this._notify();
  }

  isHostAvailable() {
    return this._hostAvailable;
  }

  setCurrentPage(pageId) {
    const validPages = ["launcher", "files", "search", "attachments", "chemistry", "diagnostics"];
    if (!validPages.includes(pageId)) {
      // eslint-disable-next-line no-console
      console.warn(`HostStore: invalid page id '${pageId}'`);
      return;
    }
    this._currentPage = pageId;
    this._notify();
  }

  getCurrentPage() {
    return this._currentPage;
  }

  snapshot() {
    return {
      hostAvailable: this._hostAvailable,
      currentPage: this._currentPage,
      envelopes: { ...this._state }
    };
  }

  // Computed: derived from envelopes, recomputed on every read
  getSessionState() {
    const sessionEnvelope = this._state.session;
    if (!sessionEnvelope || !sessionEnvelope.ok) {
      return "none";
    }
    return sessionEnvelope.data?.state ?? "none";
  }

  getActiveVaultPath() {
    const sessionEnvelope = this._state.session;
    if (!sessionEnvelope || !sessionEnvelope.ok) {
      return null;
    }
    return sessionEnvelope.data?.active_vault_path ?? null;
  }

  getLastSessionError() {
    const sessionEnvelope = this._state.session;
    if (!sessionEnvelope || !sessionEnvelope.ok) {
      return sessionEnvelope?.error ?? null;
    }
    return sessionEnvelope.data?.last_error ?? null;
  }

  getRuntimeSummary() {
    return this._state.runtime;
  }

  getBootstrapInfo() {
    return this._state.bootstrap;
  }

  classify(key) {
    return classifyEnvelope(this._state[key]);
  }
}

export const store = new HostStore();
