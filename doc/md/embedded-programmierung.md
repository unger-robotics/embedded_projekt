# Embedded-Programmierung – Grundlagen und Methodik

## 0 | Begriffe und Rahmen

**Embedded-Programmierung** bezeichnet die Entwicklung von Software für eingebettete Systeme – Computer, die in ein größeres Gerät integriert sind und dedizierte Aufgaben erfüllen (Waschmaschine, ABS-Steuergerät, dein AMR-Roboter).

**Abgrenzung zur Desktop-/Server-Programmierung:**

| Merkmal          | Desktop/Server        | Embedded                           |
|------------------|-----------------------|------------------------------------|
| Betriebssystem   | Windows, Linux (voll) | Bare-Metal, RTOS, Linux (embedded) |
| RAM              | 8–128 GB              | 64 KB – 8 MB (typisch)             |
| Speicher         | SSD/HDD, TB-Bereich   | Flash, 256 KB – 16 MB              |
| Echtzeit         | selten kritisch       | oft harte Deadlines                |
| Hardware-Zugriff | abstrahiert (Treiber) | direkt (Register, Peripherie)      |

**Zielplattform dieses Dokuments:** Mikrocontroller (MCU), speziell ESP32-S3 und vergleichbare ARM-Cortex-M-Systeme.

---

## 1 | Das Kernproblem: Begrenzte Ressourcen, harte Anforderungen

**Beobachtung:** Ein Mikrocontroller hat typischerweise 100.000× weniger RAM als ein Laptop, muss aber in Echtzeit auf physikalische Ereignisse reagieren.

**Konsequenz:** Embedded-Code muss:
1. **Deterministisch** sein – vorhersagbare Ausführungszeit
2. **Ressourcenschonend** – jedes Byte zählt
3. **Hardware-nah** – direkte Kontrolle über Peripherie
4. **Robust** – läuft oft jahrelang ohne Neustart

**Regel:** In der Embedded-Programmierung optimieren wir nicht auf Entwicklerkomfort, sondern auf Systemverhalten unter realen Bedingungen.

---

## 2 | Hardware-Software-Schnittstelle

### 2.1 | Speicherabbildung (Memory-Mapped I/O)

**Prinzip:** Peripherie (GPIO, UART, Timer) erscheint als Adressen im Speicher. Schreiben an eine Adresse ändert Hardware-Zustand.

```
Speicherkarte (vereinfacht, ESP32-S3)
────────────────────────────────────────
0x0000_0000 ┌──────────────────────────┐
            │ Boot-ROM                 │
0x3FC0_0000 ├──────────────────────────┤
            │ SRAM (512 KB)            │ ← Variablen, Stack, Heap
0x4200_0000 ├──────────────────────────┤
            │ Flash (extern, gemappt)  │ ← Code, const-Daten
0x6000_0000 ├──────────────────────────┤
            │ Peripherie-Register      │ ← GPIO, UART, SPI, I2C
            └──────────────────────────┘
```

**Beispiel – GPIO-Pin setzen (Bare-Metal):**

```c
// Adresse des GPIO-Output-Registers (ESP32-S3, vereinfacht)
#define GPIO_OUT_REG    (*(volatile uint32_t *)0x60004004)
#define GPIO_OUT_SET    (*(volatile uint32_t *)0x60004008)
#define GPIO_OUT_CLR    (*(volatile uint32_t *)0x6000400C)

// Pin 2 auf HIGH setzen
GPIO_OUT_SET = (1 << 2);

// Pin 2 auf LOW setzen
GPIO_OUT_CLR = (1 << 2);
```

**Das Schlüsselwort `volatile`:** Teilt dem Compiler mit, dass sich der Wert an dieser Adresse jederzeit ändern kann (durch Hardware oder Interrupt). Ohne `volatile` könnte der Compiler Zugriffe wegoptimieren.

### 2.2 | Register und Bitmanipulation

**Regel:** Embedded-Entwickler arbeiten ständig mit einzelnen Bits in Registern.

**Standardoperationen:**

```c
// Bit setzen (auf 1)
register |= (1 << bit_position);

// Bit löschen (auf 0)
register &= ~(1 << bit_position);

// Bit umschalten (toggle)
register ^= (1 << bit_position);

// Bit lesen
bool is_set = (register >> bit_position) & 1;
```

**Beispiel – UART-Statusregister auslesen:**

```c
#define UART_STATUS_REG   (*(volatile uint32_t *)0x60000004)
#define UART_TX_EMPTY_BIT 5
#define UART_RX_READY_BIT 0

// Warten, bis Sendepuffer leer
while (!((UART_STATUS_REG >> UART_TX_EMPTY_BIT) & 1)) {
    // busy-wait
}
```

---

## 3 | Speichermodell und Datentypen

### 3.1 | Speicherbereiche

```
┌─────────────────────────────────────────────────────────┐
│ Flash (nicht-flüchtig)                                  │
│   .text    – ausführbarer Code                          │
│   .rodata  – Konstanten, Strings (const char[])         │
├─────────────────────────────────────────────────────────┤
│ RAM (flüchtig)                                          │
│   .data    – initialisierte globale/statische Variablen │
│   .bss     – nicht-initialisierte globale/statische     │
│   Heap     – dynamische Allokation (malloc)             │
│   Stack    – lokale Variablen, Rücksprungadressen       │
└─────────────────────────────────────────────────────────┘
```

**Konsequenz für die Praxis:**

```c
// Im Flash (spart RAM):
const char VERSION[] = "1.0.0";              // .rodata
static const uint8_t LOOKUP_TABLE[256] = {}; // .rodata

// Im RAM:
char buffer[64];           // .bss (nicht initialisiert)
int counter = 0;           // .data (initialisiert)
```

### 3.2 | Datentypen mit fester Breite

**Problem:** `int` hat auf verschiedenen Plattformen unterschiedliche Größen (16, 32 oder 64 Bit).

**Lösung:** Verwende Typen aus `<stdint.h>`:

| Typ        | Größe  | Wertebereich      |
|------------|--------|-------------------|
| `uint8_t`  | 1 Byte | 0 – 255           |
| `int8_t`   | 1 Byte | −128 – 127        |
| `uint16_t` | 2 Byte | 0 – 65.535        |
| `int16_t`  | 2 Byte | −32.768 – 32.767  |
| `uint32_t` | 4 Byte | 0 – 4.294.967.295 |
| `int32_t`  | 4 Byte | ±2,147 Mrd.       |

**Regel:** In Embedded-Code verwenden wir ausschließlich Typen mit expliziter Breite, wenn Speicherlayout oder Protokolle relevant sind.

---

## 4 | Interrupts und nebenläufige Ausführung

### 4.1 | Konzept

**Definition:** Ein Interrupt ist ein Hardware-Signal, das den normalen Programmfluss unterbricht und eine spezielle Funktion (ISR, Interrupt Service Routine) ausführt.

```
Normaler Ablauf              Mit Interrupt
────────────────────         ────────────────────────────────
main() {                     main() {
  while(1) {                   while(1) {
    task_a();                    task_a();
    task_b();      ──────►       ║ ← Interrupt!
    task_c();                    ╠══► ISR ausführen
  }                              ║    (schnell!)
}                                ╚══► zurück zu task_a()
                                   task_b();
                                   task_c();
                               }
                             }
```

### 4.2 | ISR-Regeln (kritisch)

**Regel 1:** ISRs müssen kurz sein – Mikrosekunden, nicht Millisekunden.

**Regel 2:** Keine blockierenden Operationen in ISRs (kein `printf`, kein `malloc`, keine Mutexe).

**Regel 3:** Kommunikation mit Hauptprogramm über `volatile`-Variablen oder RTOS-Mechanismen.

**Beispiel – Button-Entprellung:**

```c
volatile bool button_pressed = false;  // volatile: ISR schreibt, main liest

void IRAM_ATTR button_isr_handler(void *arg) {
    button_pressed = true;  // nur Flag setzen, nichts weiter
}

void app_main(void) {
    // GPIO und Interrupt konfigurieren ...

    while (1) {
        if (button_pressed) {
            button_pressed = false;
            // Hier die eigentliche Verarbeitung
            handle_button_event();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**`IRAM_ATTR`:** Auf ESP32 müssen ISRs im RAM liegen (nicht im Flash), weil Flash-Zugriffe während Interrupts problematisch sein können.

---

## 5 | Peripherie-Kommunikation

### 5.1 | GPIO (General Purpose Input/Output)

**Grundoperationen:**

```c
// ESP-IDF Abstraktion
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << GPIO_NUM_2),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
};
gpio_config(&io_conf);

gpio_set_level(GPIO_NUM_2, 1);  // HIGH
gpio_set_level(GPIO_NUM_2, 0);  // LOW
```

### 5.2 | Serielle Protokolle (Übersicht)

| Protokoll | Leitungen               | Geschwindigkeit          | Typische Anwendung       |
|-----------|-------------------------|--------------------------|--------------------------|
| UART      | 2 (TX, RX)              | 9600 – 921600 Baud       | Debug-Konsole, GPS       |
| SPI       | 4 (MOSI, MISO, CLK, CS) | 1 – 80 MHz               | Display, SD-Karte, Flash |
| I²C       | 2 (SDA, SCL)            | 100 – 400 kHz (Standard) | Sensoren, EEPROMs        |

**Dein Selection-Panel-Projekt verwendet:**
- **SPI-ähnliche Signale** für Schieberegister (CLK, DATA, LATCH)
- **UART** für die Kommunikation mit dem Raspberry Pi

---

## 6 | Timing und Echtzeit

### 6.1 | Zeitquellen

| Methode                | Auflösung   | Anwendung                   |
|------------------------|-------------|-----------------------------|
| `vTaskDelay()`         | 1 ms (Tick) | Nicht-kritische Wartezeiten |
| Hardware-Timer         | µs – ns     | Präzise PWM, Messungen      |
| `esp_timer_get_time()` | 1 µs        | Zeitmessung, Profiling      |

### 6.2 | Echtzeit-Anforderungen

**Harte Echtzeit:** Deadline-Verletzung = Systemversagen (z. B. Airbag-Auslösung).

**Weiche Echtzeit:** Gelegentliche Verletzungen tolerierbar (z. B. Videostreaming – ein Frame-Drop ist akzeptabel).

**Dein AMR-Projekt:** Motor-Control ist weiche Echtzeit mit Ziel <10 ms Latenz; Safety-Watchdog ist tendenziell härtere Echtzeit.

---

## 7 | Fehlerbehandlung und Robustheit

### 7.1 | Defensives Programmieren

**Regel:** Vertraue keiner Eingabe – validiere Parameter, prüfe Rückgabewerte.

```c
esp_err_t sensor_read(uint8_t *data, size_t len) {
    // Parametervalidierung
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_read(...);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
```

### 7.2 | Watchdog

**Prinzip:** Ein Hardware-Timer, der das System neu startet, wenn er nicht regelmäßig „gefüttert" wird.

```c
// ESP-IDF Task Watchdog
esp_task_wdt_add(NULL);  // aktuellen Task überwachen

while (1) {
    do_work();
    esp_task_wdt_reset();  // Watchdog füttern
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

**Konsequenz:** Wenn `do_work()` hängt, startet der Watchdog das System nach Timeout (konfigurierbar, typisch 5–30 s).

---

## 8 | Entwicklungswerkzeuge

### 8.1 | Toolchain-Übersicht

```
┌─────────────────────────────────────────────────────────────┐
│ Quellcode (.c, .h)                                          │
└──────────────────────────┬──────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Compiler (xtensa-esp32s3-elf-gcc)                           │
│   → Objektdateien (.o)                                      │
└──────────────────────────┬──────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Linker                                                      │
│   → ELF-Binary mit Speicherlayout                           │
└──────────────────────────┬──────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Flash-Tool (esptool.py)                                     │
│   → Binary auf Mikrocontroller übertragen                   │
└─────────────────────────────────────────────────────────────┘
```
