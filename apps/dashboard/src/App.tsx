import { Routes, Route, Navigate } from "react-router-dom";
import { WsProvider } from "./lib/ws/store";
import Topbar from "./components/Topbar";
import DashboardPage from "./pages/DashboardPage";
import LogsPage from "./pages/LogsPage";
import SettingsPage from "./pages/SettingsPage";

export default function App() {
  return (
    <WsProvider>
      <div className="min-h-screen bg-[#0b0f14] text-slate-100">
        {/* Background (Arduino Teal + Raspi Red) */}
        <div className="pointer-events-none fixed inset-0 -z-10 bg-[radial-gradient(1200px_600px_at_20%_-10%,rgba(0,151,157,0.18),transparent_55%),radial-gradient(900px_500px_at_90%_0%,rgba(197,26,74,0.16),transparent_55%)]" />

        <Topbar />

        <main className="mx-auto max-w-[1100px] p-4">
          <Routes>
            <Route path="/" element={<DashboardPage />} />
            <Route path="/logs" element={<LogsPage />} />
            <Route path="/settings" element={<SettingsPage />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </main>

        <footer className="border-t border-white/10 px-4 py-4 text-center text-sm text-slate-400">
          Arduino Teal + Raspberry Pi Red · React SPA
        </footer>
      </div>
    </WsProvider>
  );
}
