import { cn } from "../lib/cn";

export default function Card({
  title,
  subtitle,
  children,
  className,
}: {
  title: string;
  subtitle?: string;
  children: React.ReactNode;
  className?: string;
}) {
  return (
    <section
      className={cn(
        "rounded-2xl border border-white/10 bg-[linear-gradient(180deg,rgba(16,24,38,0.9),rgba(14,21,33,0.9))] p-4 shadow-[0_10px_30px_rgba(0,0,0,0.35)]",
        className,
      )}
    >
      <div className="mb-3 flex items-baseline justify-between gap-2">
        <h2 className="text-base font-semibold">{title}</h2>
        {subtitle ? (
          <span className="text-xs text-slate-400">{subtitle}</span>
        ) : null}
      </div>
      {children}
    </section>
  );
}
