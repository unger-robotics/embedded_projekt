# FreeRTOS – Echtzeitbetriebssystem für Mikrocontroller

## 0 | Begriffe und Rahmen

**FreeRTOS** (Free Real-Time Operating System) ist ein Open-Source-Echtzeitkernel für Mikrocontroller. Er verwaltet mehrere Programmteile (Tasks), die scheinbar gleichzeitig laufen, obwohl nur ein oder zwei Prozessorkerne vorhanden sind.

**Zielplattform für dieses Dokument:** ESP32-S3 (Dual-Core, 240 MHz, Xtensa LX7).

---

## 1 | Kernproblem: Warum ein RTOS?

**Beobachtung:** Ein klassisches `while(1)`-Programm (Superloop) führt Aufgaben sequenziell aus. Wenn eine Aufgabe blockiert – etwa auf einen Sensorwert wartet –, steht das gesamte System still.

**Konsequenz:** Bei zeitkritischen Anwendungen (Motorsteuerung, Kommunikation, Sensorik) reicht Superloop nicht aus.

**Regel:** Ein RTOS teilt die CPU-Zeit in Scheiben (Zeitschlitze) und wechselt zwischen Tasks, sodass jede Aufgabe ihren Anteil erhält.

```
Superloop                         FreeRTOS-Scheduler
─────────────────────────────     ─────────────────────────────
┌──────────────────────────┐      ┌─────┐ ┌─────┐ ┌─────┐
│ Sensor lesen             │      │Task1│ │Task2│ │Task3│
│ Motor ansteuern          │  →   └──┬──┘ └──┬──┘ └──┬──┘
│ Kommunikation            │         │       │       │
│ ... alles in Reihe       │      ───┴───────┴───────┴───► Zeit
└──────────────────────────┘          Scheduler wechselt
```

---

## 2 | Zentrale Konzepte

### 2.1 | Task (Aufgabe)

**Definition:** Ein Task ist eine eigenständige Funktion mit eigenem Stack, die vom Scheduler verwaltet wird.

**Struktur:**

```c
void vMotorTask(void *pvParameters) {
    // Initialisierung (einmalig)
    motor_init();

    for (;;) {                          // Endlosschleife – Task endet nie
        motor_update();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10 ms warten, CPU freigeben
    }
}
```

**Anwendung:** Du erstellst den Task mit:

```c
xTaskCreatePinnedToCore(
    vMotorTask,       // Funktion
    "MotorTask",      // Name (Debug)
    4096,             // Stack in Bytes
    NULL,             // Parameter
    5,                // Priorität (höher = wichtiger)
    &xMotorHandle,    // Handle für spätere Steuerung
    1                 // Core (0 oder 1 beim ESP32-S3)
);
```

### 2.2 | Scheduler und Prioritäten

**Regel:** Der Scheduler aktiviert immer den Task mit der höchsten Priorität, der bereit (ready) ist.

**Task-Zustände:**

```
         ┌────────────┐
         │  Running   │ ← CPU führt diesen Task aus
         └─────┬──────┘
    Preemption │ ▲ Dispatch
               ▼ │
         ┌────────────┐
         │   Ready    │ ← wartet auf CPU-Zeit
         └─────┬──────┘
     Block     │ ▲ Unblock (Event/Timeout)
               ▼ │
         ┌────────────┐
         │  Blocked   │ ← wartet auf Ressource/Zeit
         └────────────┘
```

**Beispiel:** Ein Task mit Priorität 10 unterbricht (preemption) einen laufenden Task mit Priorität 5, sobald er ready wird.

### 2.3 | Tick und Zeitbasis

**Definition:** Der Tick ist der Herzschlag des RTOS – ein periodischer Timer-Interrupt.

| Parameter            | Typischer Wert (ESP-IDF) |
|----------------------|--------------------------|
| `configTICK_RATE_HZ` | 1000 Hz                  |
| Tick-Periode         | 1 ms                     |

**Konsequenz:** `vTaskDelay(pdMS_TO_TICKS(50))` blockiert den Task für 50 Ticks = 50 ms. Während dieser Zeit kann der Scheduler andere Tasks ausführen.

---

## 3 | Synchronisation und Kommunikation

Wenn mehrere Tasks auf gemeinsame Ressourcen zugreifen, entstehen **Race Conditions**. FreeRTOS bietet drei Mechanismen:

### 3.1 | Mutex (Mutual Exclusion)

**Zweck:** Exklusiver Zugriff auf eine Ressource.

```c
SemaphoreHandle_t xI2CMutex = xSemaphoreCreateMutex();

// In Task A und B:
if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    i2c_master_write(...);  // geschützter Bereich
    xSemaphoreGive(xI2CMutex);
}
```

**Grenze:** Mutex schützt nur, wenn *alle* beteiligten Tasks ihn verwenden.

### 3.2 | Queue (Warteschlange)

**Zweck:** Daten sicher zwischen Tasks übertragen.

```c
QueueHandle_t xSensorQueue = xQueueCreate(10, sizeof(SensorData_t));

// Sender-Task:
SensorData_t data = {.temp = 23.5, .timestamp = xTaskGetTickCount()};
xQueueSend(xSensorQueue, &data, portMAX_DELAY);

// Empfänger-Task:
SensorData_t received;
if (xQueueReceive(xSensorQueue, &received, pdMS_TO_TICKS(100)) == pdTRUE) {
    process(received);
}
```

**Vorteil:** Queues sind Thread-sicher; kein zusätzlicher Mutex nötig.

### 3.3 | Semaphore (Zähler/Binär)

**Zweck:** Signalisierung zwischen Tasks oder von ISR zu Task.

```c
SemaphoreHandle_t xButtonSem = xSemaphoreCreateBinary();

// ISR (Interrupt Service Routine):
void IRAM_ATTR button_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task:
for (;;) {
    if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) {
        handle_button_press();
    }
}
```

---

## 4 | Speicher und Stack

**Problem:** Jeder Task benötigt eigenen Stack. Zu wenig Stack führt zu Stack Overflow (undefiniertes Verhalten), zu viel verschwendet RAM.

**Faustregel für ESP32-S3:**

| Task-Typ                  | Stack-Empfehlung |
|---------------------------|------------------|
| Minimal (LED-Blinken)     | 2048 Bytes       |
| Standard (Sensorik)       | 4096 Bytes       |
| Komplex (micro-ROS, WiFi) | 8192–16384 Bytes |

**Diagnose:** `uxTaskGetStackHighWaterMark(NULL)` gibt den minimalen freien Stack in Words zurück. Bleibt weniger als 10–20 % übrig, vergrößere den Stack.

---

## 5 | ESP32-S3: Dual-Core-Besonderheiten

Der ESP32-S3 hat zwei Kerne (Core 0 und Core 1). ESP-IDF startet standardmäßig WiFi/BT auf Core 0.

**Empfehlung für dein AMR-Projekt:**

```
Core 0                          Core 1
──────────────────────────      ──────────────────────────
WiFi-Stack (System)             Motor-Control (Echtzeit)
micro-ROS Transport             Sensor-Fusion
Logging                         Safety-Watchdog
```

**Pinning:** `xTaskCreatePinnedToCore(..., 1)` bindet einen Task an Core 1. Ohne Pinning verteilt der Scheduler frei.
