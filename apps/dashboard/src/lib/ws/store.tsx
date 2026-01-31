/* eslint-disable react-refresh/only-export-components */

// store.tsx — React Context/Store fuer WebSocket (bridge.py Protokoll)
// Erwartet:
// - types.ts: export type WsCommand, WsEvent
// - client.ts: export class WsClient (emit WsClientEvent: client_* + WsEvent)

import React, {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useRef,
  useState,
} from "react";
import type { WsCommand, WsEvent } from "./types";
import { WsClient, type WsClientEvent } from "./client";

type WsState = {
  url: string;
  connected: boolean;

  // Legacy UI-Felder (bis DashboardPage/ButtonChips final umgebaut sind)
  device: string;
  fw: string;
  uptimeMs: number | null;
  lastButton: number | null;

  // hello (bridge → browser)
  btnCount: number;
  ledCount: number;
  scanPeriodMs: number | null;
  debounceMs: number | null;
  features: number | null;

  // selection (one-hot)
  selected: number; // 0..btnCount (0 = alle aus)
  source: string | null;
  tMs: number | null;

  // UI-kompatibel: bool-array fuer LED-Grid
  leds: boolean[]; // length = ledCount

  // serial status
  serialState: string; // connected | disconnected | reconnecting | ...
  serialPort: string;

  // ack/warn/error
  lastAckSeq: number | null;
  lastWarn: { code: number; detail: string } | null;
  lastError: { code: number; detail: string } | null;

  // ws reconnect info
  reconnectAttempt: number;
  reconnectInMs: number | null;

  logs: string[];
};

type WsApi = {
  state: WsState;

  setUrl: (url: string) => void;
  connect: () => void;
  disconnect: () => void;

  // Protokoll-Command
  setSelection: (selected: number) => void;

  // Legacy-Wrapper (damit vorhandene UI weiter compiliert)
  setLed: (id: number, on: boolean) => void;
  setAll: (on: boolean) => void;

  // Nicht mehr im Protokoll (Legacy/No-Op mit Log)
  requestState: () => void;
  ping: () => void;

  clearLogs: () => void;
};

const LS_KEY_URL = "sp.wsUrl";

// Fallback (falls window/import.meta nicht verfügbar sind – z.B. Tests)
const FALLBACK_URL = "ws://localhost:8787/ws";

const Ctx = createContext<WsApi | null>(null);

function fmtLog(level: string, msg: string) {
  const ts = new Date().toISOString().slice(11, 19);
  return `[${ts}] ${level.toUpperCase()}: ${msg}`;
}

function clampInt(v: number, lo: number, hi: number) {
  const n = Number.isFinite(v) ? Math.trunc(v) : lo;
  return Math.max(lo, Math.min(hi, n));
}

function oneHot(count: number, selected: number) {
  const n = Math.max(0, Math.trunc(count));
  const s = Math.trunc(selected);
  const arr = Array.from({ length: n }, () => false);
  if (s >= 1 && s <= n) arr[s - 1] = true;
  return arr;
}

function assertNever(x: never): never {
  throw new Error(`Unexpected variant: ${JSON.stringify(x)}`);
}

function wsProtoForPage(): "ws" | "wss" {
  if (typeof window === "undefined") return "ws";
  return window.location.protocol === "https:" ? "wss" : "ws";
}

function computeDefaultWsUrl(): string {
  if (typeof window === "undefined") return FALLBACK_URL;

  const proto = wsProtoForPage();
  const host = window.location.hostname;
  const port = window.location.port;

  // DEV: Vite (5173) oder preview (4173) -> Bridge-Port (default 8787) am gleichen Host
  const isViteDev = import.meta.env.DEV && (port === "5173" || port === "4173");
  if (isViteDev) {
    const bridgePort =
      (import.meta.env.VITE_BRIDGE_PORT as string | undefined) ?? "8787";
    return `${proto}://${host}:${bridgePort}/ws`;
  }

  // PROD: Bridge servt SPA -> same-origin
  return `${proto}://${window.location.host}/ws`;
}

function normalizeWsUrl(input?: string | null): string {
  const envUrl = (import.meta.env.VITE_WS_URL as string | undefined) ?? "";
  const saved = input?.trim() ?? "";
  const fallback = computeDefaultWsUrl();

  const pick = envUrl || saved || fallback;

  // Migration: wenn Seite nicht localhost ist, aber URL localhost -> auf fallback (same-origin) wechseln
  if (typeof window !== "undefined") {
    try {
      const u = new URL(pick);
      const pageHost = window.location.hostname;
      const pageIsLocal =
        pageHost === "localhost" || pageHost === "127.0.0.1";
      const urlIsLocal =
        u.hostname === "localhost" || u.hostname === "127.0.0.1";

      if (!pageIsLocal && urlIsLocal) return fallback;
    } catch {
      return fallback;
    }
  }

  return pick;
}

function loadInitialUrl(): string {
  if (typeof window === "undefined") return computeDefaultWsUrl();
  const saved = window.localStorage.getItem(LS_KEY_URL);
  return normalizeWsUrl(saved);
}

export function WsProvider({ children }: { children: React.ReactNode }) {
  const [state, setState] = useState<WsState>(() => {
    const url = loadInitialUrl();
    return {
      url,
      connected: false,

      device: "—",
      fw: "—",
      uptimeMs: null,
      lastButton: null,

      btnCount: 10,
      ledCount: 10,
      scanPeriodMs: null,
      debounceMs: null,
      features: null,

      selected: 0,
      source: null,
      tMs: null,

      leds: oneHot(10, 0),

      serialState: "unknown",
      serialPort: "—",

      lastAckSeq: null,
      lastWarn: null,
      lastError: null,

      reconnectAttempt: 0,
      reconnectInMs: null,

      logs: [fmtLog("info", `default ws url: ${url}`)],
    };
  });

  const clientRef = useRef<WsClient | null>(null);

  const pushLog = useCallback((line: string) => {
    setState((s) => ({ ...s, logs: [...s.logs, line].slice(-800) }));
  }, []);

  const handleWsEvent = useCallback(
    (evt: WsClientEvent) => {
      // client_* Events: property "t"
      if ("t" in evt) {
        switch (evt.t) {
          case "client_open":
            setState((s) => ({
              ...s,
              connected: true,
              reconnectAttempt: 0,
              reconnectInMs: null,
            }));
            pushLog(fmtLog("info", `WS connected: ${evt.data.url}`));
            return;

          case "client_close":
            setState((s) => ({ ...s, connected: false }));
            pushLog(
              fmtLog(
                "warn",
                `WS closed (${evt.data.code}): ${evt.data.reason || ""}`,
              ),
            );
            return;

          case "client_error":
            pushLog(fmtLog("error", `WS error: ${evt.data.url}`));
            return;

          case "client_reconnect_scheduled":
            setState((s) => ({
              ...s,
              reconnectAttempt: evt.data.attempt,
              reconnectInMs: evt.data.delayMs,
            }));
            pushLog(
              fmtLog(
                "warn",
                `WS reconnect in ${evt.data.delayMs} ms (attempt ${evt.data.attempt})`,
              ),
            );
            return;

          case "client_reconnect_attempt":
            setState((s) => ({
              ...s,
              reconnectAttempt: evt.data.attempt,
              reconnectInMs: null,
            }));
            pushLog(fmtLog("info", `WS reconnect attempt ${evt.data.attempt}`));
            return;

          default:
            return assertNever(evt);
        }
      }

      // Protokoll-Events: property "type"
      const e: WsEvent = evt;

      switch (e.type) {
        case "hello": {
          const btnCount = clampInt(e.btnCount, 0, 1000);
          const ledCount = clampInt(e.ledCount, 0, 1000);

          setState((s) => {
            const selected = clampInt(s.selected, 0, btnCount);
            const device = s.device === "—" ? "ESP32 (via bridge)" : s.device;

            return {
              ...s,
              device,
              btnCount,
              ledCount,
              scanPeriodMs: e.scanPeriodMs,
              debounceMs: e.debounceMs,
              features: e.features,
              selected,
              leds: oneHot(ledCount, selected),
            };
          });

          pushLog(fmtLog("info", `hello: btn=${e.btnCount} led=${e.ledCount}`));
          return;
        }

        case "selection": {
          setState((s) => {
            const selected = clampInt(e.selected, 0, s.btnCount);
            const tMs = e.tMs;

            return {
              ...s,
              selected,
              source: e.source ?? null,
              tMs,
              uptimeMs: typeof tMs === "number" ? tMs : s.uptimeMs,
              lastButton: selected > 0 ? selected : s.lastButton,
              leds: oneHot(s.ledCount, selected),
            };
          });
          return;
        }

        case "serial": {
          setState((s) => ({
            ...s,
            serialState: e.state,
            serialPort: e.port ?? "—",
          }));
          pushLog(
            fmtLog("info", `serial: ${e.state}${e.port ? ` (${e.port})` : ""}`),
          );
          return;
        }

        case "ack": {
          setState((s) => ({ ...s, lastAckSeq: e.seq }));
          pushLog(fmtLog("info", `ack: seq=${e.seq}`));
          return;
        }

        case "warn": {
          setState((s) => ({
            ...s,
            lastWarn: { code: Number(e.code), detail: e.detail },
          }));
          pushLog(fmtLog("warn", `warn ${e.code}: ${e.detail}`));
          return;
        }

        case "error": {
          setState((s) => ({
            ...s,
            lastError: { code: Number(e.code), detail: e.detail },
          }));
          pushLog(fmtLog("error", `error ${e.code}: ${e.detail}`));
          return;
        }

        default:
          return assertNever(e);
      }
    },
    [pushLog],
  );

  const api = useMemo<WsApi>(() => {
    const getClient = () => {
      if (!clientRef.current) {
        clientRef.current = new WsClient({
          url: state.url,
          onEvent: handleWsEvent,
          autoReconnect: true,
          backoff: { baseMs: 250, maxMs: 4000, factor: 2, jitterMs: 250 },
        });
      }
      return clientRef.current;
    };

    const send = (cmd: WsCommand) => getClient().send(cmd);

    return {
      state,

      setUrl: (url) => {
        const next = normalizeWsUrl(url);

        setState((s) => ({ ...s, url: next }));

        try {
          window.localStorage.setItem(LS_KEY_URL, next);
        } catch {
          // ignore
        }

        const c = getClient();
        c.setUrl(next);

        pushLog(fmtLog("info", `set url: ${next}`));
      },

      connect: () => {
        // falls url noch "alt" ist (z.B. aus saved/localStorage), vor connect normalisieren
        const next = normalizeWsUrl(state.url);
        if (next !== state.url) {
          try {
            window.localStorage.setItem(LS_KEY_URL, next);
          } catch {
            // ignore
          }
          setState((s) => ({ ...s, url: next }));
          getClient().setUrl(next);
          pushLog(fmtLog("info", `normalized url: ${next}`));
        }
        return getClient().connect();
      },

      disconnect: () => getClient().disconnect(),

      setSelection: (selected) => {
        const sel = clampInt(selected, 0, state.btnCount);
        const ok = send({ type: "setSelection", selected: sel });
        if (!ok) pushLog(fmtLog("warn", "send failed: WS not open"));
      },

      setLed: (id, on) => {
        const sel = clampInt(id, 1, state.btnCount);

        if (on) {
          const ok = send({ type: "setSelection", selected: sel });
          if (!ok) pushLog(fmtLog("warn", "send failed: WS not open"));
          return;
        }

        if (state.selected === sel) {
          const ok = send({ type: "setSelection", selected: 0 });
          if (!ok) pushLog(fmtLog("warn", "send failed: WS not open"));
        }
      },

      setAll: (on) => {
        if (!on) {
          const ok = send({ type: "setSelection", selected: 0 });
          if (!ok) pushLog(fmtLog("warn", "send failed: WS not open"));
          return;
        }
        pushLog(fmtLog("warn", "setAll(true) ignored (one-hot selection)"));
      },

      requestState: () =>
        pushLog(fmtLog("warn", "requestState not supported by protocol")),
      ping: () => pushLog(fmtLog("warn", "ping not supported by protocol")),

      clearLogs: () => setState((s) => ({ ...s, logs: [] })),
    };
  }, [state, handleWsEvent, pushLog]);

  return <Ctx.Provider value={api}>{children}</Ctx.Provider>;
}

export function useWs() {
  const v = useContext(Ctx);
  if (!v) throw new Error("useWs must be used within WsProvider");
  return v;
}
