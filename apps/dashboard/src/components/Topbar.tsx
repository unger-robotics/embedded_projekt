import { Link, useLocation } from "react-router-dom";
import StatusPill from "./StatusPill";
import { cn } from "../lib/cn";
import { useWs } from "../lib/ws/store";

function NavLink({ to, label }: { to: string; label: string }) {
  const { pathname } = useLocation();
  const active = pathname === to;

  return (
    <Link
      to={to}
      className={cn(
        "rounded-full px-3 py-2 text-sm",
        active
          ? "bg-white/10 text-slate-100"
          : "text-slate-400 hover:bg-white/5 hover:text-slate-200",
      )}
    >
      {label}
    </Link>
  );
}

export default function Topbar() {
  const { state, connect, disconnect } = useWs();

  return (
    <header className="sticky top-0 z-20 border-b border-white/10 bg-[#0b0f14]/80 backdrop-blur-md">
      <div className="mx-auto flex max-w-[1100px] items-center justify-between gap-3 px-4 py-3">
        <div className="flex items-center gap-2">
          <span
            className="h-2.5 w-2.5 rounded-full"
            style={{ background: "var(--arduino-teal)" }}
          />
          <span
            className="h-2.5 w-2.5 rounded-full"
            style={{ background: "var(--raspi-red)" }}
          />
          <span className="font-semibold">Embedded Dashboard</span>
        </div>

        <nav className="flex items-center gap-1 rounded-full border border-white/10 bg-white/5 p-1">
          <NavLink to="/" label="Dashboard" />
          <NavLink to="/logs" label="Logs" />
          <NavLink to="/settings" label="Settings" />
        </nav>

        <div className="flex items-center gap-2">
          <StatusPill connected={state.connected} />
          <button
            className="rounded-xl border border-[rgba(0,151,157,0.6)] bg-[linear-gradient(135deg,rgba(0,151,157,0.18),rgba(0,151,157,0.06))] px-3 py-2 text-sm disabled:opacity-50"
            onClick={connect}
            disabled={state.connected}
          >
            Connect
          </button>
          <button
            className="rounded-xl border border-white/10 bg-transparent px-3 py-2 text-sm disabled:opacity-50"
            onClick={disconnect}
            disabled={!state.connected}
          >
            Disconnect
          </button>
        </div>
      </div>
    </header>
  );
}
