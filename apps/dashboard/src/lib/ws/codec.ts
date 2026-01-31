// codec.ts
import type { WsCommand, WsEvent } from "./types";

function isObject(v: unknown): v is Record<string, unknown> {
  return typeof v === "object" && v !== null;
}

function readNumber(o: Record<string, unknown>, k: string): number | null {
  const v = o[k];
  return typeof v === "number" && Number.isFinite(v) ? v : null;
}

function readString(o: Record<string, unknown>, k: string): string | null {
  const v = o[k];
  return typeof v === "string" ? v : null;
}

function readOptionalString(
  o: Record<string, unknown>,
  k: string,
): string | undefined | null {
  const v = o[k];
  if (v === undefined) return undefined;
  return typeof v === "string" ? v : null;
}

/**
 * Parse WS message from bridge.py.
 * - null: unbekanntes Format/Type oder nicht parsebar
 * - WsEvent: bekanntes Event
 */
export function parseWsEvent(raw: string): WsEvent | null {
  let val: unknown;
  try {
    val = JSON.parse(raw);
  } catch {
    return null;
  }

  if (!isObject(val)) return null;

  const type = readString(val, "type");
  if (!type) return null;

  switch (type) {
    case "hello": {
      const btnCount = readNumber(val, "btnCount");
      const ledCount = readNumber(val, "ledCount");
      const scanPeriodMs = readNumber(val, "scanPeriodMs");
      const debounceMs = readNumber(val, "debounceMs");
      const features = readNumber(val, "features");

      if (
        btnCount === null ||
        ledCount === null ||
        scanPeriodMs === null ||
        debounceMs === null ||
        features === null
      ) {
        return null;
      }

      return {
        type: "hello",
        btnCount,
        ledCount,
        scanPeriodMs,
        debounceMs,
        features,
      };
    }

    case "selection": {
      const selected = readNumber(val, "selected");
      const source = readString(val, "source");
      const tMs = readNumber(val, "tMs");

      if (selected === null || source === null || tMs === null) return null;

      return { type: "selection", selected, source, tMs };
    }

    case "serial": {
      const state = readString(val, "state");
      const port = readOptionalString(val, "port");

      if (state === null) return null;
      if (port === null) return null;

      return port === undefined
        ? { type: "serial", state }
        : { type: "serial", state, port };
    }

    case "ack": {
      const seq = readNumber(val, "seq");
      if (seq === null) return null;
      return { type: "ack", seq };
    }

    case "warn": {
      const code = readNumber(val, "code");
      const detail = readString(val, "detail");
      if (code === null || detail === null) return null;
      return { type: "warn", code, detail };
    }

    case "error": {
      const code = readNumber(val, "code");
      const detail = readString(val, "detail");
      if (code === null || detail === null) return null;
      return { type: "error", code, detail };
    }

    default:
      return null;
  }
}

/** Encode command Browser → Bridge */
export function encodeWsCommand(cmd: WsCommand): string {
  if (cmd.type !== "setSelection") {
    // falls WsCommand spaeter erweitert wird, faellt es hier sofort auf
    throw new Error("encodeWsCommand: unsupported command");
  }

  if (!Number.isFinite(cmd.selected)) {
    throw new Error("encodeWsCommand: selected must be finite");
  }

  return JSON.stringify({
    type: "setSelection",
    selected: Math.trunc(cmd.selected),
  } satisfies WsCommand);
}
