import { useEffect, useMemo, useState } from "react";
import Card from "../components/Card";
import { useWs } from "../lib/ws/store";

function computeDefaultWsUrl(): string {
  // gleiche Logik wie im Store (vereinfacht, aber konsistent)
  if (typeof window === "undefined") return "ws://localhost:8787/ws";

  const proto = window.location.protocol === "https:" ? "wss" : "ws";
  const host = window.location.hostname;
  const port = window.location.port;

  const isViteDev =
    import.meta.env.DEV && (port === "5173" || port === "4173");

  if (isViteDev) {
    const bridgePort =
      (import.meta.env.VITE_BRIDGE_PORT as string | undefined) ?? "8787";
    return `${proto}://${host}:${bridgePort}/ws`;
  }

  // prod: same-origin (Bridge servt SPA)
  return `${proto}://${window.location.host}/ws`;
}

export default function SettingsPage() {
  const { state, setUrl } = useWs();

  const defaultUrl = useMemo(() => computeDefaultWsUrl(), []);
  const [value, setValue] = useState(state.url);

  // Wenn state.url (z.B. durch Auto-Migration oder Apply) geändert wird,
  // Input synchron halten – aber nicht während User tippt "springen".
  useEffect(() => {
    setValue(state.url);
  }, [state.url]);

  const dirty = value.trim() !== state.url.trim();
  const canApply = value.trim().length > 0;

  return (
    <Card title="Settings" subtitle="Client config">
      <div className="grid gap-3">
        <label className="grid gap-2 text-sm text-slate-300">
          WebSocket URL
          <input
            className="w-full rounded-xl border border-white/10 bg-white/5 px-3 py-2 font-mono text-xs text-slate-100 outline-none"
            value={value}
            onChange={(e) => setValue(e.target.value)}
            spellCheck={false}
            placeholder={defaultUrl}
          />
        </label>

        <div className="flex flex-wrap gap-2">
          <button
            className="rounded-xl border border-[rgba(0,151,157,0.6)] bg-[linear-gradient(135deg,rgba(0,151,157,0.18),rgba(0,151,157,0.06))] px-3 py-2 text-sm disabled:opacity-40"
            disabled={!canApply || !dirty}
            onClick={() => setUrl(value)}
          >
            Apply
          </button>

          <button
            className="rounded-xl border border-white/10 bg-transparent px-3 py-2 text-sm"
            onClick={() => setValue(defaultUrl)}
            title="Set input back to computed default (same-origin in prod, host+8787 in dev)"
          >
            Reset to default
          </button>

          <button
            className="rounded-xl border border-white/10 bg-transparent px-3 py-2 text-sm"
            onClick={() => setValue(state.url)}
            disabled={!dirty}
            title="Discard edits"
          >
            Revert
          </button>
        </div>

        <div className="grid gap-1 text-sm text-slate-400">
          <p>
            Active:&nbsp;
            <code className="font-mono text-xs">{state.url}</code>
          </p>
          <p>
            Default:&nbsp;
            <code className="font-mono text-xs">{defaultUrl}</code>
          </p>

          <p className="pt-1">
            Beispiele:&nbsp;
            <code className="font-mono text-xs">ws://localhost:8787/ws</code>
            &nbsp;·&nbsp;
            <code className="font-mono text-xs">
              ws://rover.local:8081/ws
            </code>
          </p>
        </div>
      </div>
    </Card>
  );
}
