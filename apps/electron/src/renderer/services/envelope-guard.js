/**
 * envelope-guard.js
 *
 * Normalizes host IPC envelopes and detects protocol gaps.
 * All host API results pass through here before entering the store.
 */

const EXPECTED_ENVELOPE_KEYS = ["ok", "data", "error"];

/**
 * @param {any} envelope
 * @returns {{ ok: boolean, data: any, error: { code: string, message: string, details: any } | null, request_id?: string }}
 */
export function sanitizeEnvelope(envelope) {
  if (!envelope || typeof envelope !== "object") {
    return {
      ok: false,
      data: null,
      error: {
        code: "RENDERER_ENVELOPE_INVALID",
        message: "Host returned a non-object envelope.",
        details: { received: typeof envelope }
      }
    };
  }

  const hasOk = typeof envelope.ok === "boolean";
  const hasData = "data" in envelope;
  const hasError = "error" in envelope;

  if (!hasOk || !hasData || !hasError) {
    return {
      ok: false,
      data: null,
      error: {
        code: "RENDERER_ENVELOPE_INCOMPLETE",
        message: "Host envelope missing required fields (ok, data, error).",
        details: { missing: EXPECTED_ENVELOPE_KEYS.filter((k) => !(k in envelope)) }
      }
    };
  }

  const normalized = {
    ok: envelope.ok,
    data: envelope.data ?? null,
    error: envelope.error ?? null
  };

  if (typeof envelope.request_id === "string") {
    normalized.request_id = envelope.request_id;
  }

  return normalized;
}

/**
 * Classify an envelope into one of four UI states.
 * @param {ReturnType<sanitizeEnvelope>} envelope
 * @param {{ hasData?: boolean }} [opts]
 * @returns {"loading" | "empty" | "unavailable" | "error" | "content"}
 */
export function classifyEnvelope(envelope, opts = {}) {
  if (!envelope) {
    return "loading";
  }

  if (!envelope.ok) {
    const code = envelope.error?.code ?? "";
    const unavailableCodes = [
      "HOST_KERNEL_ADAPTER_UNAVAILABLE",
      "HOST_IPC_INVOKE_FAILED",
      "HOST_BRIDGE_PROTOCOL_ERROR",
      "RENDERER_ENVELOPE_INVALID",
      "RENDERER_ENVELOPE_INCOMPLETE"
    ];
    if (unavailableCodes.includes(code)) {
      return "unavailable";
    }
    return "error";
  }

  const data = envelope.data;
  const isEmptyArray = Array.isArray(data) && data.length === 0;
  const isNullish = data === null || data === undefined;
  const isEmptyObject = typeof data === "object" && data !== null && Object.keys(data).length === 0;

  if (isEmptyArray || isNullish || isEmptyObject) {
    return "empty";
  }

  return "content";
}

/**
 * Report a host gap to the console. Renderer must not polyfill.
 * @param {string} gapId
 * @param {string} description
 */
export function reportHostGap(gapId, description) {
  // eslint-disable-next-line no-console
  console.warn(`[EXPLICIT-HOST-GAP] ${gapId}: ${description}`);
}
