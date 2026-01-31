// src/components/ButtonChips.tsx
import { useWs } from "../lib/ws/store";

export default function ButtonChips() {
  const { state, setSelection } = useWs();

  const selected = state.selected;
  const last = state.lastButton;

  // dynamisch (falls später 100)
  const count = Math.max(0, Math.trunc(state.btnCount ?? 0));
  const ids = Array.from({ length: count }, (_, i) => i + 1);

  return (
    <div className="flex flex-wrap gap-2">
      {ids.map((id) => {
        const isSelected = selected === id;

        // "lastButton" nur als Hinweis, wenn aktuell keine Selection aktiv ist
        const isLast =
          !isSelected && selected === 0 && last != null && last === id;

        return (
          <button
            key={id}
            type="button"
            disabled={!state.connected}
            aria-pressed={isSelected}
            title={
              !state.connected
                ? "not connected"
                : isSelected
                  ? "clear selection (0)"
                  : `select BTN ${id}`
            }
            onClick={() => setSelection(isSelected ? 0 : id)}
            className={[
              "rounded-full border px-2.5 py-1 font-mono text-xs",
              "transition-colors disabled:opacity-50",
              isSelected
                ? "border-[rgba(197,26,74,0.65)] bg-[rgba(197,26,74,0.20)] text-slate-100"
                : isLast
                  ? "border-[rgba(197,26,74,0.35)] bg-[rgba(197,26,74,0.10)] text-slate-200/90"
                  : "border-white/10 bg-white/5 text-slate-400",
            ].join(" ")}
          >
            {`BTN ${id}`}
          </button>
        );
      })}
    </div>
  );
}
