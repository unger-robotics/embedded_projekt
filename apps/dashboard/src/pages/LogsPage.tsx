import Card from "../components/Card";
import { useWs } from "../lib/ws/store";
import { useEffect, useRef, useState } from "react";

export default function LogsPage() {
  const { state, clearLogs } = useWs();
  const [autoScroll, setAutoScroll] = useState(true);
  const ref = useRef<HTMLPreElement | null>(null);

  useEffect(() => {
    if (!autoScroll) return;
    const el = ref.current;
    if (!el) return;
    el.scrollTop = el.scrollHeight;
  }, [state.logs, autoScroll]);

  return (
    <Card title="Logs" subtitle="log · error · status">
      <div className="mb-3 flex items-center justify-between gap-2">
        <button
          className="rounded-xl border border-white/10 bg-transparent px-3 py-2 text-sm"
          onClick={clearLogs}
        >
          Clear
        </button>
        <label className="flex items-center gap-2 text-sm text-slate-400">
          <input
            type="checkbox"
            checked={autoScroll}
            onChange={(e) => setAutoScroll(e.target.checked)}
          />
          Auto-scroll
        </label>
      </div>

      <pre
        ref={ref}
        className="max-h-[64vh] min-h-[420px] overflow-auto rounded-xl border border-white/10 bg-black/25 p-3 font-mono text-xs leading-relaxed break-words whitespace-pre-wrap"
      >
        {state.logs.join("\n")}
      </pre>
    </Card>
  );
}
