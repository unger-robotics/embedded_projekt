// src/main.cpp
#include <Arduino.h>

namespace app_loop {
void setup();
void loop();
} // namespace app_loop

namespace app_task {
void setup();
void loop();
} // namespace app_task

void setup() {
#if APP_USE_FREERTOS
    app_task::setup();
#else
    app_loop::setup();
#endif
}

void loop() {
#if APP_USE_FREERTOS
    app_task::loop();
#else
    app_loop::loop();
#endif
}
