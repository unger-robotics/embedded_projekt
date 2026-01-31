/* ==========================================================================
   Schematic JavaScript v7.7
   Stand: 2026-01-17
   Funktionen: Tab-Navigation, Interaktive LED/Button-Steuerung
   Farbschema: CSS-gesteuert (Arduino Teal + Raspberry Pi)
   Konsistent mit HARDWARE.md und Firmware v2.7.0
   NEU: Klickbare Buttons mit One-Hot LED-Verhalten
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
    initTabs();
    initInteractiveLEDs();
});

/* --------------------------------------------------------------------------
   State Management - Simuliert das echte System
   -------------------------------------------------------------------------- */
const state = {
    selectedChannel: 0,  // 0 = keiner, 1-10 = Kanal
    autoMode: false,     // Auto-Animation an/aus
    autoInterval: null
};

/* --------------------------------------------------------------------------
   Tab Navigation
   -------------------------------------------------------------------------- */
function initTabs() {
    const tabs = document.querySelectorAll('.tab');
    const views = document.querySelectorAll('.view');
    
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const viewId = tab.dataset.view;
            
            // Deactivate all
            tabs.forEach(t => t.classList.remove('active'));
            views.forEach(v => v.classList.remove('active'));
            
            // Activate selected
            tab.classList.add('active');
            document.getElementById(viewId)?.classList.add('active');
        });
    });
}

/* --------------------------------------------------------------------------
   Interactive LEDs & Buttons
   One-hot Verhalten: Immer nur eine LED aktiv (wie im echten System)
   Policy: LowestIdWins bei gleichzeitigem Druck (hier nicht relevant)
   -------------------------------------------------------------------------- */
function initInteractiveLEDs() {
    const leds = document.querySelectorAll('.led');
    const buttons = document.querySelectorAll('.btn-symbol');
    
    if (leds.length === 0 || buttons.length === 0) return;
    
    // Buttons klickbar machen
    buttons.forEach((btn, index) => {
        const channelId = index + 1;
        
        // Data-Attribut für Kanal-ID
        btn.dataset.channel = channelId;
        
        // Klick-Handler
        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            setSelection(channelId);
        });
        
        // Hover-Effekt per CSS (cursor wird in CSS gesetzt)
        btn.style.cursor = 'pointer';
    });
    
    // LEDs auch klickbar (Toggle)
    leds.forEach((led, index) => {
        const channelId = index + 1;
        led.dataset.channel = channelId;
        led.style.cursor = 'pointer';
        
        led.addEventListener('click', (e) => {
            e.stopPropagation();
            // Toggle: Wenn schon aktiv → aus, sonst an
            if (state.selectedChannel === channelId) {
                setSelection(0);
            } else {
                setSelection(channelId);
            }
        });
    });
    
    // Status-Anzeige erstellen
    createStatusDisplay();
    
    // Keyboard-Shortcuts für Kanäle
    document.addEventListener('keydown', handleChannelKeys);
    
    // Initial: Kanal 0 (alle aus)
    updateLEDs();
}

/* --------------------------------------------------------------------------
   Selection setzen (One-Hot)
   -------------------------------------------------------------------------- */
function setSelection(channelId) {
    // Auto-Mode stoppen bei manuellem Eingriff
    if (state.autoMode) {
        toggleAutoMode();
    }
    
    state.selectedChannel = channelId;
    updateLEDs();
    updateStatusDisplay();
    
    // Log wie im echten System
    console.log(`[selection] channel=${channelId}, source=local_btn`);
}

/* --------------------------------------------------------------------------
   LEDs aktualisieren (One-Hot)
   -------------------------------------------------------------------------- */
function updateLEDs() {
    const leds = document.querySelectorAll('.led');
    const buttons = document.querySelectorAll('.btn-symbol');
    
    // Alle LEDs aus
    leds.forEach(led => led.classList.remove('active'));
    
    // Alle Buttons inaktiv
    buttons.forEach(btn => btn.classList.remove('pressed'));
    
    // Wenn ein Kanal ausgewählt ist
    if (state.selectedChannel > 0 && state.selectedChannel <= leds.length) {
        const activeLed = leds[state.selectedChannel - 1];
        const activeBtn = buttons[state.selectedChannel - 1];
        
        if (activeLed) activeLed.classList.add('active');
        if (activeBtn) activeBtn.classList.add('pressed');
    }
}

/* --------------------------------------------------------------------------
   Status-Display erstellen
   -------------------------------------------------------------------------- */
function createStatusDisplay() {
    // Status-Element in Header einfügen
    const statusBar = document.querySelector('.status-bar');
    if (!statusBar) return;
    
    // Interaktiver Status
    const statusItem = document.createElement('div');
    statusItem.className = 'status-item status-interactive';
    statusItem.innerHTML = `
        <span class="dot dot-cyan"></span>
        <span id="channel-display">sel: 00</span>
        <button id="auto-toggle" class="auto-btn" title="Auto-Animation (A)">▶</button>
        <button id="clear-btn" class="clear-btn" title="Alle aus (0)">⏹</button>
    `;
    statusBar.appendChild(statusItem);
    
    // Event-Handler für Buttons
    document.getElementById('auto-toggle')?.addEventListener('click', toggleAutoMode);
    document.getElementById('clear-btn')?.addEventListener('click', () => setSelection(0));
    
    // Styling inline hinzufügen (falls CSS nicht geladen)
    const style = document.createElement('style');
    style.textContent = `
        .status-interactive {
            gap: 0.5rem !important;
        }
        .auto-btn, .clear-btn {
            background: var(--bg-tertiary, #2D3A45);
            border: 1px solid var(--border, #30363D);
            color: var(--text-primary, #E6EDF3);
            padding: 0.1rem 0.4rem;
            border-radius: 3px;
            cursor: pointer;
            font-size: 0.6rem;
        }
        .auto-btn:hover, .clear-btn:hover {
            border-color: var(--arduino-teal, #00979D);
        }
        .auto-btn.active {
            background: var(--arduino-teal, #00979D);
            color: white;
        }
        #channel-display {
            font-family: 'SF Mono', Monaco, monospace;
            min-width: 50px;
        }
    `;
    document.head.appendChild(style);
}

/* --------------------------------------------------------------------------
   Status-Display aktualisieren
   -------------------------------------------------------------------------- */
function updateStatusDisplay() {
    const display = document.getElementById('channel-display');
    if (display) {
        const ch = state.selectedChannel.toString().padStart(2, '0');
        display.textContent = `sel: ${ch}`;
    }
}

/* --------------------------------------------------------------------------
   Auto-Animation Toggle
   -------------------------------------------------------------------------- */
function toggleAutoMode() {
    state.autoMode = !state.autoMode;
    
    const btn = document.getElementById('auto-toggle');
    if (btn) {
        btn.classList.toggle('active', state.autoMode);
        btn.textContent = state.autoMode ? '⏸' : '▶';
    }
    
    if (state.autoMode) {
        // Animation starten
        let currentChannel = state.selectedChannel || 1;
        
        state.autoInterval = setInterval(() => {
            currentChannel = (currentChannel % 10) + 1;
            state.selectedChannel = currentChannel;
            updateLEDs();
            updateStatusDisplay();
        }, 500);
    } else {
        // Animation stoppen
        if (state.autoInterval) {
            clearInterval(state.autoInterval);
            state.autoInterval = null;
        }
    }
}

/* --------------------------------------------------------------------------
   Keyboard Shortcuts für Kanäle
   -------------------------------------------------------------------------- */
function handleChannelKeys(e) {
    // Nur wenn kein Input-Feld fokussiert ist
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    
    // 0-9 für Kanäle (0 = alle aus, 1-9 = Kanal 1-9)
    if (e.key >= '0' && e.key <= '9') {
        const channel = parseInt(e.key);
        // 0 = alle aus, 1-9 = Kanal
        if (channel === 0) {
            setSelection(0);
        } else {
            setSelection(channel);
        }
    }
    
    // Shift+0 für Kanal 10
    if (e.key === '!' || (e.shiftKey && e.key === '1')) {
        // Auf deutschen Tastaturen ist Shift+1 = !
    }
    
    // A für Auto-Toggle
    if (e.key === 'a' || e.key === 'A') {
        toggleAutoMode();
    }
    
    // Space für nächsten Kanal
    if (e.key === ' ') {
        e.preventDefault();
        const next = (state.selectedChannel % 10) + 1;
        setSelection(next);
    }
    
    // Backspace für vorherigen Kanal
    if (e.key === 'Backspace') {
        e.preventDefault();
        const prev = state.selectedChannel <= 1 ? 10 : state.selectedChannel - 1;
        setSelection(prev);
    }
    
    // Escape für alle aus
    if (e.key === 'Escape') {
        setSelection(0);
    }
}

/* --------------------------------------------------------------------------
   Utility: Copy Pin Table to Clipboard
   -------------------------------------------------------------------------- */
function copyPinTable() {
    const table = document.querySelector('.card-primary .pin-table');
    if (!table) return;
    
    let text = 'ESP32-S3 XIAO Pinbelegung\n';
    text += '═'.repeat(40) + '\n';
    
    table.querySelectorAll('tbody tr').forEach(row => {
        const cells = row.querySelectorAll('td');
        text += `${cells[0].textContent.padEnd(4)} ${cells[1].textContent.padEnd(12)} ${cells[2].textContent}\n`;
    });
    
    navigator.clipboard.writeText(text).then(() => {
        console.log('Pin table copied to clipboard');
    });
}

/* --------------------------------------------------------------------------
   Debug: Log Current State
   -------------------------------------------------------------------------- */
function logState() {
    console.log('Schematic State:', {
        selectedChannel: state.selectedChannel,
        autoMode: state.autoMode,
        ledCount: document.querySelectorAll('.led').length,
        btnCount: document.querySelectorAll('.btn-symbol').length
    });
}

/* --------------------------------------------------------------------------
   Legacy Keyboard Shortcuts (Tab-Wechsel)
   -------------------------------------------------------------------------- */
document.addEventListener('keydown', (e) => {
    // Ctrl+1-5 für Tab-Wechsel (mit Ctrl um Konflikte zu vermeiden)
    if (e.ctrlKey) {
        if (e.key === '1') document.querySelector('[data-view="prototype"]')?.click();
        if (e.key === '2') document.querySelector('[data-view="full"]')?.click();
        if (e.key === '3') document.querySelector('[data-view="timing"]')?.click();
        if (e.key === '4') document.querySelector('[data-view="pinout-595"]')?.click();
        if (e.key === '5') document.querySelector('[data-view="pinout-4021"]')?.click();
    }
    
    // D für Debug
    if (e.key === 'd' && e.ctrlKey) {
        e.preventDefault();
        logState();
    }
    
    // C für Copy Pin Table
    if (e.key === 'c' && e.ctrlKey && e.shiftKey) {
        e.preventDefault();
        copyPinTable();
    }
});
