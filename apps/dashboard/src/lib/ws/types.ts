// types.ts
// WebSocket JSON (Browser ⇄ bridge.py) — Quelle: PROTOCOL.md
// Browser → Bridge: { "type":"setSelection", "selected": 3 }
// Bridge → Browser: hello | selection | serial | ack | warn | error

export type WsCommand = SetSelectionCmd;

export type SetSelectionCmd = {
  type: "setSelection";
  /**
   * 0..btnCount (0 bedeutet: alle aus)
   * Clamp/Validation kann clientseitig erfolgen, Bridge clamp’t ebenfalls.
   */
  selected: number;
};

// -----------------------------
// Bridge → Browser (Events)
// -----------------------------

export type SelectionSource = "local_btn" | "host_cmd" | "reset" | "recover";

export type HelloEvent = {
  type: "hello";
  btnCount: number;
  ledCount: number;
  scanPeriodMs: number;
  debounceMs: number;
  features: number;
};

export type SelectionEvent = {
  type: "selection";
  selected: number; // 0..btnCount
  source: SelectionSource | string; // forward compatible
  tMs: number;
};

export type SerialState = "connected" | "disconnected" | "reconnecting";

export type SerialEvent = {
  type: "serial";
  state: SerialState | string; // forward compatible
  port?: string;
};

export type AckEvent = {
  type: "ack";
  seq: number;
};

export const WARN = {
  FRAME_TOO_LONG: 11,
  ACK_TIMEOUT: 20,
  SERIAL_RECONNECT: 40,
} as const;

export type WarnCode = (typeof WARN)[keyof typeof WARN] | number;

export const ERROR = {
  INVALID_JSON: 2,
  FRAME_OVER_MAX: 12,
  ACK_FAILED: 21,
  SERIAL_SEND_FAILED: 22,
  SERIAL_NOT_CONNECTED: 30,
} as const;

export type ErrorCode = (typeof ERROR)[keyof typeof ERROR] | number;

export type WarnEvent = {
  type: "warn";
  code: WarnCode;
  detail: string;
};

export type ErrorEvent = {
  type: "error";
  code: ErrorCode;
  detail: string;
};

export type WsEvent =
  | HelloEvent
  | SelectionEvent
  | SerialEvent
  | AckEvent
  | WarnEvent
  | ErrorEvent;

export function isWsEvent(v: unknown): v is WsEvent {
  if (typeof v !== "object" || v === null) return false;
  if (!("type" in v)) return false;
  const t = (v as Record<string, unknown>).type;
  return typeof t === "string";
}
