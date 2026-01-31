# React-Stack Grundlagen: Frontend-Entwicklung für Embedded-Systeme

## 0) Begriffsrahmen und Zielsetzung

### Anwendungskontext

Diese Dokumentation richtet sich an Embedded-Entwickler, die ein Browser-basiertes Dashboard für Hardware-Projekte erstellen. Der konkrete Anwendungsfall ist das Selection-Panel-Frontend, das über WebSocket mit einem Python-Backend kommuniziert und den Zustand von 10–100 LEDs visualisiert. Der moderne React-Stack ersetzt manuelles HTML/CSS/JavaScript durch ein wartbares, typsicheres System.

### Lernziele

Nach dem Durcharbeiten dieses Dokuments kannst du:

1. **Das React-Komponentenmodell erklären** – verstehen, warum deklarative UIs skalierbarer sind als DOM-Manipulation
2. **TypeScript-Typen definieren** – Interfaces für Props, State und WebSocket-Nachrichten schreiben
3. **Vite konfigurieren** – Entwicklungsserver starten, Proxies einrichten, Production-Builds erstellen
4. **Code-Qualität automatisieren** – ESLint und Prettier integrieren
5. **Client-seitige Navigation implementieren** – React Router ohne WebSocket-Unterbrechung nutzen
6. **Tailwind CSS anwenden** – Utility-First Styling für Hardware-UIs

### Begriffsdefinitionen

| Begriff          | Definition                                                        | Quelle/Kontext        |
|------------------|-------------------------------------------------------------------|-----------------------|
| **React**        | JavaScript-Bibliothek für komponentenbasierte, deklarative UIs    | Meta (Facebook), 2013 |
| **TypeScript**   | JavaScript-Erweiterung mit statischer Typisierung                 | Microsoft, 2012       |
| **Vite**         | Build-Tool und Entwicklungsserver (frz. „vite" = schnell)         | Evan You, 2020        |
| **JSX**          | JavaScript XML – HTML-ähnliche Syntax in JavaScript               | React-Spezifikation   |
| **ESLint**       | Statische Code-Analyse zur Fehlererkennung vor Laufzeit           | OpenJS Foundation     |
| **Prettier**     | Code-Formatter für einheitlichen Stil                             | prettier.io           |
| **React Router** | Client-seitige Navigation ohne Seitenneuladen                     | Remix Software        |
| **Tailwind CSS** | Utility-First CSS-Framework mit vordefinierten Klassen            | Tailwind Labs         |
| **HMR**          | Hot Module Replacement – Code-Änderungen ohne Seitenneuladen      | Webpack/Vite          |
| **Props**        | Properties – Eingabeparameter einer React-Komponente              | React-Terminologie    |
| **State**        | Lokaler Zustand einer Komponente, der Re-Renders auslöst          | React-Terminologie    |
| **Hook**         | Funktion zum Einbinden von React-Features in Funktionskomponenten | React 16.8+           |

---

## 1) React – Komponenten statt DOM-Manipulation

### Problemstellung

Bei klassischem JavaScript manipulierst du das DOM direkt – jede Zustandsänderung erfordert manuelle Updates aller betroffenen Elemente:

```javascript
// Klassisch: imperativ, fehleranfällig bei komplexen Zuständen
document.getElementById('led-5').classList.add('active');
document.getElementById('status').textContent = 'LED 5 an';
// Und was ist mit dem Counter? Dem Log? Der Statusleiste?
```

Bei 100 LEDs mit verschiedenen Zuständen (aktiv, inaktiv, blinkend, Fehler) wird dieser Ansatz unwartbar. Wenn sich ein Wert ändert, musst du manuell alle betroffenen DOM-Elemente finden und aktualisieren.

### Regel

React kehrt das Prinzip um: Du beschreibst den *gewünschten Zustand* (deklarativ), React berechnet die nötigen DOM-Änderungen (Reconciliation).

### Herleitung: Imperativ vs. Deklarativ

**Imperatives Paradigma (klassisches JS):**

```
Zustand ändert sich → Entwickler findet betroffene DOM-Elemente → Entwickler aktualisiert jedes Element
```

**Deklaratives Paradigma (React):**

```
Zustand ändert sich → React vergleicht alten/neuen Virtual DOM → React aktualisiert nur geänderte Elemente
```

*Analogie:* Imperativ ist wie ein Koch, der jeden Handgriff einzeln anweist („Nimm das Messer, schneide die Zwiebel, dann..."). Deklarativ ist wie ein Rezept („Am Ende soll eine geschnittene Zwiebel vorliegen"). Die Analogie endet dort, wo es um Parallelität geht: React kann mehrere Änderungen batchen, ein Koch nicht.

### Anwendung: Komponenten-Architektur

**Grundstruktur einer Komponente:**

```tsx
// Eine Komponente ist eine Funktion: Props → JSX
interface LedButtonProps {
  id: number;
  isActive: boolean;
  onPress: (id: number) => void;
}

function LedButton({ id, isActive, onPress }: LedButtonProps) {
  return (
    <button
      className={isActive ? 'led active' : 'led'}
      onClick={() => onPress(id)}
    >
      LED {id}
    </button>
  );
}
```

**Kernkonzepte:**

| Konzept         | Bedeutung                                   | Beispiel                       |
|-----------------|---------------------------------------------|--------------------------------|
| **Props**       | Eingabeparameter (read-only, von außen)     | `id`, `isActive`, `onPress`    |
| **JSX**         | HTML-ähnliche Syntax, wird zu JS kompiliert | `<button>...</button>`         |
| **Komposition** | Komponenten enthalten andere Komponenten    | `LedPanel` enthält `LedButton` |

### Anwendung: State und Hooks

Lokaler Zustand wird mit dem `useState`-Hook verwaltet:

```tsx
function LedPanel() {
  // State: Set von aktiven LED-IDs
  const [activeIds, setActiveIds] = useState<Set<number>>(new Set());

  // Event-Handler: Toggle einer LED
  const handlePress = (id: number) => {
    setActiveIds(prev => {
      const next = new Set(prev);
      next.has(id) ? next.delete(id) : next.add(id);
      return next;
    });
  };

  // Render: 100 LedButtons in einem Grid
  return (
    <div className="grid grid-cols-10">
      {Array.from({ length: 100 }, (_, i) => (
        <LedButton
          key={i}
          id={i}
          isActive={activeIds.has(i)}
          onPress={handlePress}
        />
      ))}
    </div>
  );
}
```

**Ablauf bei Button-Klick:**

```
1. User klickt LED 42
2. onClick ruft handlePress(42)
3. setActiveIds erzeugt neues Set mit/ohne 42
4. React erkennt State-Änderung
5. React rendert nur LedButton mit id=42 neu
6. Die anderen 99 Buttons bleiben unberührt (Memoization)
```

### Anwendung im Selection Panel

Wenn der ESP32 über WebSocket meldet „LED 42 aktiv", aktualisierst du nur den State:

```tsx
function useSelectionState() {
  const [selectedId, setSelectedId] = useState<number | null>(null);

  // WebSocket-Listener
  useEffect(() => {
    const ws = new WebSocket('ws://rover.local:8081/ws');
    ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      if (msg.type === 'selection') {
        setSelectedId(msg.selected);  // Nur diese Zeile!
      }
    };
    return () => ws.close();
  }, []);

  return selectedId;
}
```

React rendert automatisch nur die betroffenen Komponenten neu – nicht die anderen 99 LEDs.

### Zusammenfassung

> **Merkregel:** „State ist die Wahrheit, UI ist die Ableitung. Ändere den State, React ändert das DOM."

---

## 2) TypeScript – Fehler vor Laufzeit finden

### Problemstellung

JavaScript-Fehler treten erst zur Laufzeit auf – im schlimmsten Fall beim Benutzer:

```javascript
// JavaScript: Fehler erst im Browser sichtbar
function setLedState(id, state) {
  websocket.send({ led: id, state: state });
}

setLedState("5", "an");  // Kompiliert, crasht zur Laufzeit
// TypeError: Expected number, got string
```

Bei einem 100-LED-Panel mit verschiedenen Message-Typen multiplizieren sich die Fehlermöglichkeiten.

### Regel

TypeScript prüft Typen beim Kompilieren. Fehler erscheinen im Editor, bevor du speicherst – nicht erst beim Benutzer.

### Herleitung: Statische vs. Dynamische Typisierung

| Aspekt          | JavaScript (dynamisch)     | TypeScript (statisch)      |
|-----------------|----------------------------|----------------------------|
| Typprüfung      | Zur Laufzeit               | Beim Kompilieren           |
| Fehlererkennung | Im Browser, beim User      | Im Editor, beim Entwickler |
| Refactoring     | Gefährlich (stille Fehler) | Sicher (Compiler warnt)    |
| Dokumentation   | Nur Kommentare             | Typen sind Dokumentation   |

### Anwendung: Typen für das Selection Panel

**Interface für Komponenten-Props:**

```typescript
// Props sind der Vertrag zwischen Parent und Child
interface LedGridProps {
  leds: Map<number, boolean>;
  selectedId: number | null;
  onLedClick: (id: number) => void;
}

function LedGrid({ leds, selectedId, onLedClick }: LedGridProps) {
  // TypeScript garantiert: leds ist eine Map, nicht undefined
  // TypeScript garantiert: onLedClick ist eine Funktion
}
```

**Discriminated Unions für WebSocket-Nachrichten:**

```typescript
// Jede Nachricht hat einen type, der die Struktur bestimmt
type WsMessage =
  | { type: 'selection'; selected: number; source: string }
  | { type: 'button_press'; id: number; timestamp: number }
  | { type: 'led_update'; id: number; state: boolean }
  | { type: 'error'; code: number; message: string };

function handleMessage(msg: WsMessage) {
  switch (msg.type) {
    case 'selection':
      // TypeScript weiß: msg hat .selected und .source
      console.log(`Selection: ${msg.selected} from ${msg.source}`);
      break;
    case 'error':
      // TypeScript weiß: msg hat .code und .message
      console.error(`Error ${msg.code}: ${msg.message}`);
      break;
  }
}
```

**Typ-Sicherheit bei API-Aufrufen:**

```typescript
interface LedCommand {
  led: number;
  state: boolean;
}

function sendLedCommand(ws: WebSocket, cmd: LedCommand): void {
  ws.send(JSON.stringify(cmd));
}

// Kompiliert NICHT:
sendLedCommand(ws, { led: "5", state: "an" });
// Error: Type 'string' is not assignable to type 'number'
// Error: Type 'string' is not assignable to type 'boolean'

// Kompiliert:
sendLedCommand(ws, { led: 5, state: true });
```

### Anwendung: Konfiguration

**tsconfig.json (strenge Einstellungen):**

```json
{
  "compilerOptions": {
    "target": "ES2020",
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "strict": true,
    "noImplicitAny": true,
    "strictNullChecks": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true
  }
}
```

| Option             | Wirkung                                     |
|--------------------|---------------------------------------------|
| `strict`           | Aktiviert alle strikten Prüfungen           |
| `noImplicitAny`    | Verbietet untypisierte Variablen            |
| `strictNullChecks` | `null` und `undefined` müssen explizit sein |
| `noUnusedLocals`   | Warnt bei ungenutzten Variablen             |

### Zusammenfassung

> **Merkregel:** „Der Compiler ist dein erster Tester. Jeder Typ-Fehler, den er findet, ist ein Bug weniger beim User."

---

## 3) Vite – Build-Tool und Entwicklungsserver

### Problemstellung

Browser verstehen weder TypeScript noch JSX direkt. Der Quellcode muss transformiert werden:

```
Quellcode (.tsx)  →  [Transformation]  →  Browser-Code (.js)
   TypeScript           Compiler            JavaScript
   JSX                  Bundler             HTML-kompatibel
   Imports              Tree-Shaking        Optimiert
```

Ältere Tools wie Webpack benötigen für diese Transformation mehrere Sekunden bis Minuten bei großen Projekten.

### Regel

Vite nutzt native ES-Module des Browsers und transformiert nur geänderte Dateien. Das Ergebnis: Sub-100ms Hot Module Replacement.

### Herleitung: Webpack vs. Vite

| Aspekt                 | Webpack (traditionell) | Vite (modern)                 |
|------------------------|------------------------|-------------------------------|
| Startzeit (Dev)        | 10–60 Sekunden         | < 1 Sekunde                   |
| HMR-Geschwindigkeit    | 1–5 Sekunden           | < 100 ms                      |
| Bundling (Dev)         | Alles im Voraus        | On-Demand (lazy)              |
| Browser-Kompatibilität | Polyfills nötig        | Moderne Browser vorausgesetzt |

**Warum ist Vite schneller?**

```
Webpack (Dev):
  Alle Dateien → Bundle → Browser lädt 1 große Datei

Vite (Dev):
  Browser fordert Datei an → Vite transformiert on-the-fly → Browser lädt
```

Vite transformiert nur, was der Browser gerade braucht.

### Anwendung: Projekt erstellen und starten

**Neues Projekt:**

```bash
npm create vite@latest dashboard -- --template react-ts
cd dashboard
npm install
```

**Entwicklungsmodus:**

```bash
npm run dev
# → http://localhost:5173/
# → Hot Module Replacement aktiv
# → Änderungen erscheinen in < 100 ms
```

**Produktions-Build:**

```bash
npm run build
# → Optimierte Dateien in dist/
# → Minifiziert, Tree-Shaken, Code-Split
```

### Anwendung: Konfiguration für Selection Panel

**vite.config.ts:**

```typescript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],

  server: {
    port: 5173,
    // Proxy: WebSocket-Anfragen an Bridge weiterleiten
    proxy: {
      '/ws': {
        target: 'ws://localhost:8081',
        ws: true,
        changeOrigin: true
      }
    }
  },

  build: {
    outDir: 'dist',
    sourcemap: false,  // Produktion: keine Source Maps
    minify: 'esbuild'  // Schnellste Minifizierung
  }
});
```

**Proxy-Erklärung:**

```
Browser (localhost:5173)  ──► /ws  ──► Vite Proxy  ──► bridge.py (localhost:8081)
```

So kannst du während der Entwicklung gegen die echte Bridge testen, ohne CORS-Probleme.

### Zwei Modi im Vergleich

| Aspekt      | `npm run dev`                | `npm run build`              |
|-------------|------------------------------|------------------------------|
| Ziel        | Entwicklung                  | Produktion                   |
| Output      | Kein Dateisystem, nur Memory | `dist/` mit statischen Files |
| Optimierung | Keine (schnellster Start)    | Voll (kleinste Dateien)      |
| Source Maps | Ja (für Debugging)           | Nein (für Sicherheit)        |
| HMR         | Ja                           | N/A                          |

### Zusammenfassung

> **Merkregel:** „npm run dev zum Entwickeln (schnell), npm run build vor Deploy (optimiert). Proxy löst CORS."

---

## 4) ESLint + Prettier – Code-Qualität automatisieren

### Problemstellung

Inkonsistenter Code erschwert Reviews und Zusammenarbeit:

```javascript
// Entwickler A
function foo(){return 42}

// Entwickler B
function bar() {
    return 42;
}
```

Zusätzlich bleiben potenzielle Bugs unentdeckt, bis sie zur Laufzeit crashen.

### Regel

ESLint findet problematische Muster (Analyse), Prettier erzwingt einheitlichen Stil (Formatierung). Beide arbeiten zusammen, aber haben getrennte Verantwortlichkeiten.

### Herleitung: Analyse vs. Formatierung

| Tool     | Aufgabe                             | Beispiel-Regeln                       |
|----------|-------------------------------------|---------------------------------------|
| ESLint   | Fehler finden, Best Practices       | `no-unused-vars`, `react-hooks/rules` |
| Prettier | Formatierung (Whitespace, Klammern) | Einrückung, Semikolons, Zeilenlänge   |

**Warum zwei Tools?** ESLint ist konfigurierbar bis ins Detail, Prettier hat bewusst wenige Optionen („opinionated"). Die Trennung verhindert Konflikte.

### Anwendung: ESLint einrichten

**Installation:**

```bash
npm install -D eslint @typescript-eslint/parser @typescript-eslint/eslint-plugin
npm install -D eslint-plugin-react eslint-plugin-react-hooks
```

**eslint.config.js (Flat Config, ESLint 9+):**

```javascript
import tsParser from '@typescript-eslint/parser';
import tsPlugin from '@typescript-eslint/eslint-plugin';
import reactPlugin from 'eslint-plugin-react';
import hooksPlugin from 'eslint-plugin-react-hooks';

export default [
  {
    files: ['**/*.{ts,tsx}'],
    languageOptions: {
      parser: tsParser,
      parserOptions: {
        ecmaFeatures: { jsx: true }
      }
    },
    plugins: {
      '@typescript-eslint': tsPlugin,
      'react': reactPlugin,
      'react-hooks': hooksPlugin
    },
    rules: {
      // TypeScript
      '@typescript-eslint/no-unused-vars': 'error',
      '@typescript-eslint/no-explicit-any': 'warn',

      // React
      'react/jsx-uses-react': 'off',  // React 17+ braucht kein Import
      'react/react-in-jsx-scope': 'off',

      // Hooks
      'react-hooks/rules-of-hooks': 'error',
      'react-hooks/exhaustive-deps': 'warn'
    }
  }
];
```

### Anwendung: Prettier einrichten

**Installation:**

```bash
npm install -D prettier eslint-config-prettier
```

**.prettierrc:**

```json
{
  "semi": true,
  "singleQuote": true,
  "tabWidth": 2,
  "trailingComma": "es5",
  "printWidth": 100,
  "bracketSpacing": true,
  "jsxSingleQuote": false
}
```

| Option          | Wert    | Wirkung                           |
|-----------------|---------|-----------------------------------|
| `semi`          | `true`  | Semikolons am Zeilenende          |
| `singleQuote`   | `true`  | Einfache Anführungszeichen in JS  |
| `tabWidth`      | `2`     | 2 Leerzeichen Einrückung          |
| `trailingComma` | `"es5"` | Trailing Commas wo ES5 es erlaubt |
| `printWidth`    | `100`   | Maximale Zeilenlänge              |

### Anwendung: VS Code Integration

**.vscode/settings.json:**

```json
{
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "esbenp.prettier-vscode",
  "editor.codeActionsOnSave": {
    "source.fixAll.eslint": "explicit"
  },
  "[typescript]": {
    "editor.defaultFormatter": "esbenp.prettier-vscode"
  },
  "[typescriptreact]": {
    "editor.defaultFormatter": "esbenp.prettier-vscode"
  }
}
```

**Workflow beim Speichern:**

```
1. Du speicherst eine Datei (Ctrl+S / Cmd+S)
2. Prettier formatiert automatisch (Whitespace, Klammern)
3. ESLint fixt automatisch fixbare Probleme
4. Verbleibende ESLint-Fehler erscheinen als Unterstreichungen
```

### npm-Scripts

**package.json:**

```json
{
  "scripts": {
    "lint": "eslint src/",
    "lint:fix": "eslint src/ --fix",
    "format": "prettier --write src/",
    "typecheck": "tsc --noEmit"
  }
}
```

**Vor jedem Commit:**

```bash
npm run lint && npm run typecheck && npm run build
```

### Zusammenfassung

> **Merkregel:** „ESLint findet Bugs, Prettier formatiert. Beim Speichern automatisch, vor Commit manuell prüfen."

---

## 5) React Router – Client-seitige Navigation

### Problemstellung

Bei klassischen Webseiten lädt jeder Seitenwechsel die komplette Seite neu:

```
User klickt Link → Browser fordert neue HTML-Seite an → Kompletter Neuaufbau
```

Für das Selection Panel bedeutet das: Der WebSocket würde bei jedem Seitenwechsel getrennt und müsste neu verbunden werden. Der aktuelle LED-Zustand ginge verloren.

### Regel

React Router wechselt nur die Komponenten innerhalb der Seite, ohne den Browser neu zu laden. Die WebSocket-Verbindung bleibt bestehen.

### Herleitung: Server-side vs. Client-side Routing

| Aspekt          | Server-side Routing | Client-side Routing (React Router) |
|-----------------|---------------------|------------------------------------|
| Seitenwechsel   | Kompletter Reload   | Nur Komponenten-Austausch          |
| Netzwerk        | Neue HTML-Anfrage   | Keine (JS läuft weiter)            |
| Zustand (State) | Verloren            | Erhalten                           |
| WebSocket       | Getrennt            | Verbunden                          |
| URL-Änderung    | Browser lädt neu    | History API (kein Reload)          |

### Anwendung: Basis-Setup

**Installation:**

```bash
npm install react-router-dom
```

**App.tsx mit Routing:**

```tsx
import { BrowserRouter, Routes, Route, Link } from 'react-router-dom';
import { DashboardPage } from './pages/DashboardPage';
import { SettingsPage } from './pages/SettingsPage';
import { DebugPage } from './pages/DebugPage';

function App() {
  return (
    <BrowserRouter>
      {/* Navigation – bleibt bei jedem Route-Wechsel sichtbar */}
      <nav className="flex gap-4 p-4 bg-gray-800">
        <Link to="/" className="text-white hover:text-blue-400">
          Dashboard
        </Link>
        <Link to="/settings" className="text-white hover:text-blue-400">
          Einstellungen
        </Link>
        <Link to="/debug" className="text-white hover:text-blue-400">
          Debug
        </Link>
      </nav>

      {/* Nur dieser Bereich ändert sich beim Routing */}
      <main className="p-4">
        <Routes>
          <Route path="/" element={<DashboardPage />} />
          <Route path="/settings" element={<SettingsPage />} />
          <Route path="/debug" element={<DebugPage />} />
        </Routes>
      </main>
    </BrowserRouter>
  );
}
```

### Anwendung: WebSocket oberhalb des Routers

**Kritisch für Selection Panel:** Der WebSocket-Hook muss oberhalb des `<BrowserRouter>` leben, damit die Verbindung bei Navigation erhalten bleibt:

```tsx
import { createContext, useContext } from 'react';
import { useBridgeWebSocket } from './hooks/useBridgeWebSocket';

// Context für globalen WebSocket-Zugriff
const WsContext = createContext<ReturnType<typeof useBridgeWebSocket> | null>(null);

export function useWs() {
  const ctx = useContext(WsContext);
  if (!ctx) throw new Error('useWs must be used within WsProvider');
  return ctx;
}

function App() {
  // WebSocket-Hook auf oberster Ebene
  const ws = useBridgeWebSocket('ws://rover.local:8081/ws');

  return (
    <WsContext.Provider value={ws}>
      <BrowserRouter>
        <nav>...</nav>
        <Routes>
          <Route path="/" element={<DashboardPage />} />
          {/* Alle Pages können useWs() nutzen */}
        </Routes>
      </BrowserRouter>
    </WsContext.Provider>
  );
}
```

**Komponenten-Hierarchie:**

```
App
├── WsContext.Provider    ← WebSocket lebt hier (persistent)
│   └── BrowserRouter
│       ├── <nav>         ← Immer sichtbar
│       └── <Routes>
│           ├── /         → DashboardPage (mounted/unmounted)
│           ├── /settings → SettingsPage  (mounted/unmounted)
│           └── /debug    → DebugPage     (mounted/unmounted)
```

Wenn du von `/` zu `/settings` navigierst, wird `DashboardPage` unmounted und `SettingsPage` mounted – aber `WsContext.Provider` bleibt. Die WebSocket-Verbindung ist davon nicht betroffen.

### Zusammenfassung

> **Merkregel:** „WebSocket über Router, nicht in Router. Navigation wechselt Seiten, nicht Verbindungen."

---

## 6) Tailwind CSS – Utility-First Styling

### Problemstellung

Traditionelles CSS erfordert separate Dateien und erfundene Klassennamen:

```css
/* styles.css */
.led-button { /* ... */ }
.led-button--active { /* ... */ }
.led-button--disabled { /* ... */ }
.led-button--blinking { /* ... */ }
```

Bei 100 LEDs mit verschiedenen Zuständen wächst die CSS-Datei unkontrolliert. Spezifitätskonflikte entstehen, Refactoring wird riskant.

### Regel

Tailwind bietet vordefinierte Utility-Klassen direkt im Markup. Du erfindest keine Klassennamen, sondern kombinierst Utilities.

### Herleitung: BEM vs. Utility-First

| Aspekt               | BEM (traditionell)               | Utility-First (Tailwind)         |
|----------------------|----------------------------------|----------------------------------|
| Klassennamen         | Selbst erfunden (`.led-button`)  | Vordefiniert (`bg-green-500`)    |
| CSS-Dateien          | Wachsen mit Features             | Konstant (nur genutzte Klassen)  |
| Spezifität           | Konflikte möglich                | Alle Utilities gleich spezifisch |
| Komponenten-Kopplung | CSS + HTML in getrennten Dateien | Alles im Markup                  |

### Anwendung: Setup

**Installation:**

```bash
npm install -D tailwindcss postcss autoprefixer
npx tailwindcss init -p
```

**tailwind.config.js:**

```javascript
/** @type {import('tailwindcss').Config} */
export default {
  content: [
    './index.html',
    './src/**/*.{js,ts,jsx,tsx}'
  ],
  theme: {
    extend: {
      colors: {
        'led-active': '#22c55e',      // Grün für aktive LED
        'led-inactive': '#374151',    // Grau für inaktive LED
        'led-error': '#ef4444'        // Rot für Fehler
      },
      animation: {
        'pulse-led': 'pulse 1s ease-in-out infinite'
      }
    }
  },
  plugins: []
};
```

**src/index.css:**

```css
@tailwind base;
@tailwind components;
@tailwind utilities;
```

### Anwendung: LED-Button-Komponente

```tsx
interface LedButtonProps {
  id: number;
  isActive: boolean;
  isSelected: boolean;
  onClick: (id: number) => void;
}

function LedButton({ id, isActive, isSelected, onClick }: LedButtonProps) {
  return (
    <button
      onClick={() => onClick(id)}
      className={`
        w-12 h-12 rounded-full
        transition-all duration-150
        flex items-center justify-center
        text-xs font-mono
        ${isActive
          ? 'bg-led-active shadow-lg shadow-green-500/50'
          : 'bg-led-inactive hover:bg-gray-600'}
        ${isSelected
          ? 'ring-2 ring-white ring-offset-2 ring-offset-gray-900'
          : ''}
      `}
    >
      {id}
    </button>
  );
}
```

**Utility-Klassen erklärt:**

| Klasse                | CSS-Äquivalent                        |
|-----------------------|---------------------------------------|
| `w-12`                | `width: 3rem` (48px)                  |
| `h-12`                | `height: 3rem`                        |
| `rounded-full`        | `border-radius: 9999px`               |
| `transition-all`      | `transition-property: all`            |
| `duration-150`        | `transition-duration: 150ms`          |
| `shadow-lg`           | Großer Box-Shadow                     |
| `shadow-green-500/50` | Shadow-Farbe grün mit 50% Opacity     |
| `ring-2`              | 2px Outline (Focus-Ring)              |
| `ring-offset-2`       | 2px Abstand zwischen Ring und Element |

### Anwendung: Grid für 100 LEDs

```tsx
function LedGrid({ leds, selectedId, onLedClick }: LedGridProps) {
  return (
    <div className="
      grid grid-cols-10 gap-2
      p-4 bg-gray-900 rounded-lg
      max-w-fit mx-auto
    ">
      {Array.from({ length: 100 }, (_, i) => (
        <LedButton
          key={i}
          id={i + 1}
          isActive={leds.get(i + 1) ?? false}
          isSelected={selectedId === i + 1}
          onClick={onLedClick}
        />
      ))}
    </div>
  );
}
```

**Grid-Klassen:**

| Klasse         | Wirkung                       |
|----------------|-------------------------------|
| `grid`         | Display: grid                 |
| `grid-cols-10` | 10 Spalten                    |
| `gap-2`        | 0.5rem Abstand zwischen Items |
| `max-w-fit`    | Breite passt sich Inhalt an   |
| `mx-auto`      | Horizontales Zentrieren       |

### Zusammenfassung

> **Merkregel:** „Keine CSS-Dateien mehr. Utilities im Markup, Theme in der Config. Build entfernt Ungenutztes."

---

## 7) Zusammenspiel im Selection-Panel-Projekt

### Entwicklungsumgebung (Mac)

```
┌────────────────────────────────────────────────────────────────────────┐
│  ENTWICKLUNG (Mac)                                                     │
│                                                                        │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐              │
│  │   VS Code    │    │  Vite Dev    │    │   ESLint/    │              │
│  │ + Extensions │───►│ Server :5173 │◄───│   Prettier   │              │
│  └──────────────┘    └──────┬───────┘    └──────────────┘              │
│         │                   │                                          │
│    Live Edit          Proxy /ws                                        │
│         │                   │                                          │
│         ▼                   ▼                                          │
│  ┌──────────────────────────────────────┐                              │
│  │          bridge.py :8081             │◄──── USB-CDC ────► ESP32-S3  │
│  │    (läuft auf Mac oder Pi)           │                              │
│  └──────────────────────────────────────┘                              │
└────────────────────────────────────────────────────────────────────────┘
```

**Workflow:**

1. `npm run dev` startet Vite auf `:5173`
2. Browser öffnet `http://localhost:5173`
3. WebSocket-Anfragen (`/ws`) werden zu `:8081` geproxied
4. Änderungen im Code → HMR → Browser aktualisiert in < 100 ms
5. ESLint/Prettier formatieren beim Speichern

### Produktionsumgebung (Raspberry Pi)

```
┌────────────────────────────────────────────────────────────────────────┐
│  PRODUKTION (Raspberry Pi)                                             │
│                                                                        │
│  ┌──────────────────────────────────────┐                              │
│  │          bridge.py :8081             │                              │
│  │                                      │                              │
│  │  ├─ HTTP: liefert dist/              │                              │
│  │  └─ WS: /ws für Echtzeit-Updates     │                              │
│  └──────────────────┬───────────────────┘                              │
│                     │                                                  │
│                USB-CDC Serial                                          │
│                     │                                                  │
│                     ▼                                                  │
│              ┌──────────────┐                                          │
│              │   ESP32-S3   │                                          │
│              │  + CD4021B   │                                          │
│              │  + 74HC595   │                                          │
│              └──────────────┘                                          │
└────────────────────────────────────────────────────────────────────────┘
```

**Deployment:**

```bash
# 1. Lokal bauen
cd apps/dashboard
npm run lint && npm run typecheck && npm run build

# 2. Auf Pi kopieren
rsync -avh --delete \
  ./dist/ \
  pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/

# 3. Service neu starten (optional, wenn nur Frontend geändert)
ssh pi@rover "sudo systemctl restart embedded-bridge.service"
```

### Datenfluss: Button-Druck → Browser-Update

```
1. User drückt Button 7 am Panel
   └─► CD4021B → ESP32 Firmware

2. Firmware sendet SELECTION_STATE
   └─► Serial (COBS/CRC) → bridge.py

3. Bridge wandelt in JSON
   └─► {"type":"selection","selected":7}

4. WebSocket pusht zu allen Clients
   └─► Browser empfängt via useBridgeWs()

5. React State-Update
   └─► setSelectedId(7)

6. React rendert nur LED 7 neu
   └─► CSS-Klasse wechselt zu 'bg-led-active'

Gesamtlatenz: 40–75 ms
```
