import { cn } from "../lib/cn";

export default function StatusPill({ connected }: { connected: boolean }) {
  return (
    <span
      className={cn(
        "rounded-full border px-2.5 py-1 font-mono text-xs",
        connected
          ? "border-emerald-400/40 bg-emerald-400/10 text-emerald-100"
          : "border-rose-400/40 bg-rose-400/10 text-rose-100",
      )}
    >
      {connected ? "CONNECTED" : "DISCONNECTED"}
    </span>
  );
}
