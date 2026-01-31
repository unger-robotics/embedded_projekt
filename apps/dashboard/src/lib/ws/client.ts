// client.ts — WebSocket Client mit Backoff-Reconnect
//
// Erwartet:
// - types.ts: export type WsCommand, WsEvent
// - codec.ts: export function parseWsEvent(raw: string): WsEvent | null
//             export function encodeWsCommand(cmd: WsCommand): string

import type { WsCommand, WsEvent } from "./types";
import { encodeWsCommand, parseWsEvent } from "./codec";

export type ClientOpenEvt = { t: "client_open"; data: { url: string } };
export type ClientCloseEvt = {
  t: "client_close";
  data: { url: string; code: number; reason: string };
};
export type ClientErrorEvt = { t: "client_error"; data: { url: string } };

export type ClientReconnectScheduledEvt = {
  t: "client_reconnect_scheduled";
  data: { url: string; attempt: number; delayMs: number };
};

export type ClientReconnectAttemptEvt = {
  t: "client_reconnect_attempt";
  data: { url: string; attempt: number };
};

export type WsClientEvent =
  | WsEvent
  | ClientOpenEvt
  | ClientCloseEvt
  | ClientErrorEvt
  | ClientReconnectScheduledEvt
  | ClientReconnectAttemptEvt;

export type WsClientOptions = {
  url: string;
  onEvent: (evt: WsClientEvent) => void;

  autoReconnect?: boolean;

  backoff?: {
    baseMs?: number; // default 250
    maxMs?: number; // default 4000
    factor?: number; // default 2
    jitterMs?: number; // default 250
  };
};

type BackoffCfg = Required<NonNullable<WsClientOptions["backoff"]>>;

export class WsClient {
  private ws: WebSocket | null = null;
  private url: string;

  private autoReconnect: boolean;
  private backoff: BackoffCfg;

  private reconnectTimer: number | null = null;
  private attempt = 0;

  private wantReconnect = false; // "soll verbunden bleiben" (connect gedrueckt)
  private closingExplicitly = false; // verhindert Backoff bei absichtlichem close

  constructor(opts: WsClientOptions) {
    this.url = opts.url;
    this.onEvent = opts.onEvent;

    this.autoReconnect = opts.autoReconnect ?? true;
    this.backoff = {
      baseMs: opts.backoff?.baseMs ?? 250,
      maxMs: opts.backoff?.maxMs ?? 4000,
      factor: opts.backoff?.factor ?? 2,
      jitterMs: opts.backoff?.jitterMs ?? 250,
    };
  }

  onEvent: WsClientOptions["onEvent"];

  setUrl(url: string) {
    const next = url.trim();
    if (!next || next === this.url) return;

    this.url = next;

    // Wenn du schon verbunden bist/verbinden willst: sauberer Reconnect auf neue URL
    if (this.wantReconnect) this.reconnectNow("url change");
  }

  connect() {
    this.wantReconnect = true;
    this.closingExplicitly = false;

    this.clearReconnectTimer();
    this.attempt = 0;

    this.openSocket();
  }

  disconnect() {
    this.wantReconnect = false;
    this.closingExplicitly = true;

    this.clearReconnectTimer();

    if (!this.ws) return;
    try {
      this.ws.close(1000, "client disconnect");
    } catch {
      // ignore
    }
  }

  isOpen() {
    return !!this.ws && this.ws.readyState === WebSocket.OPEN;
  }

  send(cmd: WsCommand) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return false;
    this.ws.send(encodeWsCommand(cmd));
    return true;
  }

  // Optional helper (nice fuer UI/Store)
  setSelection(selected: number) {
    return this.send({ type: "setSelection", selected });
  }

  // -------------------------
  // Internals
  // -------------------------

  private openSocket() {
    if (
      this.ws &&
      (this.ws.readyState === WebSocket.OPEN ||
        this.ws.readyState === WebSocket.CONNECTING)
    )
      return;

    const url = this.url;
    const ws = new WebSocket(url);
    this.ws = ws;

    ws.addEventListener("open", () => {
      // Reset Backoff nach erfolgreichem Connect
      this.attempt = 0;
      this.onEvent({ t: "client_open", data: { url } });
    });

    ws.addEventListener("close", (e) => {
      // Nur reagieren, wenn das unser aktuelles Socket ist
      if (this.ws === ws) this.ws = null;

      this.onEvent({
        t: "client_close",
        data: { url, code: e.code, reason: e.reason },
      });

      const explicit = this.closingExplicitly;
      this.closingExplicitly = false;

      if (explicit) return;
      if (!this.autoReconnect) return;
      if (!this.wantReconnect) return;

      this.scheduleReconnect();
    });

    ws.addEventListener("error", () => {
      this.onEvent({ t: "client_error", data: { url } });
      // Reconnect triggern wir im close-Handler (Browser feuert close meist nach error).
    });

    ws.addEventListener("message", (m) => {
      const evt = parseWsEvent(String(m.data));
      if (!evt) return;
      this.onEvent(evt);
    });
  }

  private reconnectNow(reason: string) {
    if (!this.autoReconnect) return;

    this.clearReconnectTimer();
    this.attempt = 0;

    // Wenn ein Socket offen/connecting ist: schliessen und dann (im close-Handler) neu verbinden.
    // Damit vermeiden wir parallele Verbindungen.
    if (
      this.ws &&
      (this.ws.readyState === WebSocket.OPEN ||
        this.ws.readyState === WebSocket.CONNECTING)
    ) {
      this.closingExplicitly = true;
      try {
        this.ws.close(1000, reason);
      } catch {
        // ignore
      }
      // Direkt danach neu oeffnen, weil wir "explicit close" gesetzt haben (kein Backoff).
      // Wichtig: close-Event kommt async; wir starten trotzdem neu, wenn readyState CLOSED wird.
      // Sauberer: kurz warten, aber hier "simple is best":
      window.setTimeout(() => this.openSocket(), 0);
      return;
    }

    this.openSocket();
  }

  private scheduleReconnect() {
    this.attempt = Math.min(this.attempt + 1, 30);

    const { baseMs, maxMs, factor, jitterMs } = this.backoff;
    const exp = baseMs * Math.pow(factor, Math.max(0, this.attempt - 1));
    const jitter = Math.floor(Math.random() * (jitterMs + 1));
    const delayMs = Math.min(maxMs, Math.floor(exp)) + jitter;

    const url = this.url;
    this.onEvent({
      t: "client_reconnect_scheduled",
      data: { url, attempt: this.attempt, delayMs },
    });

    this.clearReconnectTimer();
    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = null;

      if (!this.autoReconnect) return;
      if (!this.wantReconnect) return;

      this.onEvent({
        t: "client_reconnect_attempt",
        data: { url: this.url, attempt: this.attempt },
      });
      this.openSocket();
    }, delayMs);
  }

  private clearReconnectTimer() {
    if (this.reconnectTimer === null) return;
    window.clearTimeout(this.reconnectTimer);
    this.reconnectTimer = null;
  }
}
