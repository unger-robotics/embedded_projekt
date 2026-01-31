import Card from "../components/Card";
import LedGrid from "../components/LedGrid";
import ButtonChips from "../components/ButtonChips";
import { useWs } from "../lib/ws/store";

export default function DashboardPage() {
  const { state } = useWs();

  return (
    <div className="grid grid-cols-1 gap-3 lg:grid-cols-[1.1fr_1.1fr_0.9fr]">
      <Card title="Status" subtitle="WebSocket · Bridge/Serial · Selection">
        <div className="grid gap-2">
          <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
            <span className="text-sm text-slate-400">Server</span>
            <span className="font-mono text-xs">{state.url}</span>
          </div>

          <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
            <span className="text-sm text-slate-400">WebSocket</span>
            <span className="font-mono text-xs">
              {state.connected ? "connected" : "disconnected"}
            </span>
          </div>

          <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
            <span className="text-sm text-slate-400">Serial</span>
            <span className="font-mono text-xs">
              {state.serialState}
              {state.serialPort && state.serialPort !== "—"
                ? ` · ${state.serialPort}`
                : ""}
            </span>
          </div>

          <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
            <span className="text-sm text-slate-400">Selection</span>
            <span className="font-mono text-xs">
              {state.selected}
              {state.source ? ` · ${state.source}` : ""}
            </span>
          </div>

          <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
            <span className="text-sm text-slate-400">Uptime</span>
            <span className="font-mono text-xs">
              {state.uptimeMs == null
                ? "—"
                : `${Math.round(state.uptimeMs / 1000)} s`}
            </span>
          </div>

          <div className="grid grid-cols-2 gap-2">
            <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
              <span className="text-sm text-slate-400">Buttons</span>
              <span className="font-mono text-xs">{state.btnCount}</span>
            </div>
            <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
              <span className="text-sm text-slate-400">LEDs</span>
              <span className="font-mono text-xs">{state.ledCount}</span>
            </div>
          </div>

          <div className="grid grid-cols-2 gap-2">
            <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
              <span className="text-sm text-slate-400">scanPeriod</span>
              <span className="font-mono text-xs">
                {state.scanPeriodMs == null ? "—" : `${state.scanPeriodMs} ms`}
              </span>
            </div>
            <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
              <span className="text-sm text-slate-400">debounce</span>
              <span className="font-mono text-xs">
                {state.debounceMs == null ? "—" : `${state.debounceMs} ms`}
              </span>
            </div>
          </div>

          <div className="flex items-center justify-between rounded-xl border border-white/10 bg-white/5 p-3">
            <span className="text-sm text-slate-400">lastAck</span>
            <span className="font-mono text-xs">
              {state.lastAckSeq == null ? "—" : state.lastAckSeq}
            </span>
          </div>

          {state.lastWarn && (
            <div className="rounded-xl border border-amber-400/20 bg-amber-400/10 p-3">
              <div className="flex items-center justify-between">
                <span className="text-sm text-amber-200">
                  warn {state.lastWarn.code}
                </span>
                <span className="font-mono text-xs text-amber-100/90">
                  {state.lastWarn.detail}
                </span>
              </div>
            </div>
          )}

          {state.lastError && (
            <div className="rounded-xl border border-red-400/20 bg-red-400/10 p-3">
              <div className="flex items-center justify-between">
                <span className="text-sm text-red-200">
                  error {state.lastError.code}
                </span>
                <span className="font-mono text-xs text-red-100/90">
                  {state.lastError.detail}
                </span>
              </div>
            </div>
          )}

          {!state.connected && state.reconnectAttempt > 0 && (
            <div className="rounded-xl border border-white/10 bg-white/5 p-3 text-sm text-slate-300">
              Reconnect attempt {state.reconnectAttempt}
              {state.reconnectInMs != null
                ? ` · in ${state.reconnectInMs} ms`
                : ""}
            </div>
          )}
        </div>
      </Card>

      <Card title="LEDs" subtitle='One-hot · WS: {"type":"setSelection"}'>
        <LedGrid />
      </Card>

      <Card title="Buttons" subtitle="letzte Auswahl (selection)">
        <ButtonChips />
        <p className="mt-3 text-sm leading-relaxed text-slate-400">
          Die Bridge broadcastet selection (selected/source/tMs). Daraus leiten
          wir lastButton und Uptime ab.
        </p>
      </Card>
    </div>
  );
}
