/**
 * @file    console.cpp
 * @brief   Serial Monitor command handler for testing.
 */
#include "console.h"
#include "config.h"
#include "debug.h"
#include "display.h"

#include "station.h"

#if DEBUG_ENABLED && DEBUG_CONSOLE

namespace {

constexpr uint8_t BRIGHTNESS_STEP = 20;

void printHelp() {
  DEBUG_PRINTLN(F("---------------- Commands ----------------"));
  DEBUG_PRINTLN(F(" m       screen mode: AUTO/CLOCK/PLANE/RADAR/FORECAST/ROTATE"));
  DEBUG_PRINTLN(F(" r       download radar now"));
  DEBUG_PRINTLN(F(" + / -   brightness up / down (until next change)"));
  DEBUG_PRINTLN(F(" l       latch blanking 1..8 (test against flicker)"));
  DEBUG_PRINTLN(F(" d       pin drive strength 0..3 (test against WiFi trouble)"));
  DEBUG_PRINTLN(F(" i       system info"));
  DEBUG_PRINTLN(F(" h       this help"));
  DEBUG_PRINTLN(F("------------------------------------------"));
}

void printInfo() {
  DEBUG_LOG("INFO", "%s %s", FW_NAME, FW_VERSION);
  DEBUG_LOG("INFO", "Chip %s rev %d, %lu MHz",
            ESP.getChipModel(), ESP.getChipRevision(),
            (unsigned long)ESP.getCpuFreqMHz());
  DEBUG_LOG("INFO", "Heap free %lu B (largest %lu B), PSRAM free %lu B",
            (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap(),
            (unsigned long)ESP.getFreePsram());
  DEBUG_LOG("INFO", "Panel %dx%d, brightness %u",
            Display::width(), Display::height(), Display::brightness());
  Station::printInfo();
}

void changeBrightness(int delta) {
  int value = constrain((int)Display::brightness() + delta, 0, 255);
  Display::setBrightness((uint8_t)value);
}

bool handleModeKey(char c) {
  switch (c) {
    case 'm': Station::cycleMode();    return true;
    case 'r': Station::refreshRadar(); return true;
  }
  return false;
}

}  // namespace

namespace Console {

void begin() {
  printHelp();
}

void update() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (handleModeKey(c)) continue;
    switch (c) {
      case '+': changeBrightness(+BRIGHTNESS_STEP); break;
      case '-': changeBrightness(-BRIGHTNESS_STEP); break;
      case 'i': printInfo();                        break;
      case 'l': Display::setLatchBlanking(Display::latchBlanking() % 8 + 1); break;
      case 'd': Display::setDriveStrength((Display::driveStrength() + 1) % 4); break;
      case 'h': case '?': printHelp();              break;
      default:
        DEBUG_LOG("CONS", "Unknown command '%c', press h for help", c);
        break;
    }
  }
}

}  // namespace Console

#else  // console disabled – empty stubs, removed by the compiler

namespace Console {
  void begin()  {}
  void update() {}
}

#endif
