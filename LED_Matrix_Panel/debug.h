/**
 * @file    debug.h
 * @brief   Debug macros for the Serial Monitor, controlled by DEBUG_ENABLED.
 *
 * Usage:
 *   DEBUG_BEGIN();                                   // once, in setup()
 *   DEBUG_LOG("DISP", "Brightness set to %u", 80);   // printf-style
 *
 * Output format:
 *   [   12345] [DISP] Brightness set to 80
 *    ^ millis()  ^ module tag
 *
 * When DEBUG_ENABLED is 0, every macro expands to nothing, so the debug
 * strings are not even stored in flash.
 */
#pragma once

#include "config.h"

#if DEBUG_ENABLED

  // Start the serial port and give the Serial Monitor a moment to attach.
  #define DEBUG_BEGIN()                                   \
    do {                                                  \
      Serial.begin(DEBUG_BAUDRATE);                       \
      delay(1000);                                        \
      Serial.println();                                   \
    } while (0)

  // Tagged, timestamped log line (adds newline automatically).
  #define DEBUG_LOG(tag, fmt, ...)                        \
    Serial.printf("[%8lu] [%-4s] " fmt "\n",              \
                  (unsigned long)millis(), tag, ##__VA_ARGS__)

  // Raw line for special cases (help table, separators).
  #define DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)

#else

  #define DEBUG_BEGIN()             do {} while (0)
  #define DEBUG_LOG(tag, fmt, ...)  do {} while (0)
  #define DEBUG_PRINTLN(...)        do {} while (0)

#endif
