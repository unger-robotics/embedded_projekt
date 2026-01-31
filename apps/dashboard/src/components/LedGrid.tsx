import { useWs } from "../lib/ws/store";

type LedTheme = {
  pillOn: string;
  pillOff: string;
  cardOn: string;
  cardOff: string;
};

function themeForLed(id: number): LedTheme {
  // Zuordnung:
  // 1-2 weiß, 3-4 blau, 5-6 rot, 7-8 gelb, 9-10 grün
  if (id <= 2) {
    // white
    return {
      cardOn: "border-slate-200/30 bg-slate-200/5",
      cardOff: "border-white/10 bg-white/5",
      pillOn: "border-slate-200/50 bg-slate-200/20",
      pillOff: "border-white/15 bg-white/5",
    };
  }

  if (id <= 4) {
    // blue
    return {
      cardOn: "border-sky-400/25 bg-sky-400/5",
      cardOff: "border-white/10 bg-white/5",
      pillOn: "border-sky-400/55 bg-sky-400/18",
      pillOff: "border-white/15 bg-white/5",
    };
  }

  if (id <= 6) {
    // red (wie BTN-Markierung)
    return {
      cardOn: "border-[rgba(197,26,74,0.28)] bg-[rgba(197,26,74,0.06)]",
      cardOff: "border-white/10 bg-white/5",
      pillOn: "border-[rgba(197,26,74,0.55)] bg-[rgba(197,26,74,0.18)]",
      pillOff: "border-white/15 bg-white/5",
    };
  }

  if (id <= 8) {
    // yellow
    return {
      cardOn: "border-amber-400/22 bg-amber-400/5",
      cardOff: "border-white/10 bg-white/5",
      pillOn: "border-amber-400/55 bg-amber-400/16",
      pillOff: "border-white/15 bg-white/5",
    };
  }

  // green (9-10)
  return {
    cardOn: "border-emerald-400/22 bg-emerald-400/5",
    cardOff: "border-white/10 bg-white/5",
    pillOn: "border-emerald-400/55 bg-emerald-400/16",
    pillOff: "border-white/15 bg-white/5",
  };
}

export default function LedGrid() {
  const { state, setSelection } = useWs();

  const ledCount = Math.min(
    state.ledCount ?? state.leds.length,
    state.leds.length,
  );

  return (
    <div className="grid gap-3">
      <div className="grid grid-cols-5 gap-2">
        {state.leds.slice(0, ledCount).map((on, i) => {
          const id = i + 1;
          const t = themeForLed(id);

          return (
            <div
              key={id}
              className={[
                "flex items-center justify-between rounded-xl border px-3 py-2",
                "transition-colors",
                on ? t.cardOn : t.cardOff,
                !state.connected ? "opacity-60" : "",
              ].join(" ")}
            >
              <span className="font-mono text-xs text-slate-400">{`LED ${id}`}</span>

              {/* Anzeige: keine Klicks, kein weißer Knopf-Kreis */}
              <div
                className={[
                  "h-7 w-12 rounded-full border",
                  "transition-colors",
                  on ? t.pillOn : t.pillOff,
                ].join(" ")}
                role="switch"
                aria-checked={on}
                aria-label={`LED ${id} (derived from selection)`}
                title={
                  !state.connected
                    ? "not connected"
                    : on
                      ? `on (derived from selection ${state.selected})`
                      : "off (no selection)"
                }
              />
            </div>
          );
        })}
      </div>

      <div className="flex flex-wrap gap-2">
        <button
          className="rounded-xl border border-white/10 bg-transparent px-3 py-2 text-sm disabled:opacity-50"
          onClick={() => setSelection(0)}
          disabled={!state.connected}
          title={!state.connected ? "not connected" : "clear selection (0)"}
        >
          All off
        </button>

        <button
          className="rounded-xl border border-white/10 bg-transparent px-3 py-2 text-sm opacity-50"
          disabled
          title="One-hot selection: es gibt nur genau eine Auswahl oder 0 (= alle aus)"
        >
          All on
        </button>
      </div>
    </div>
  );
}
