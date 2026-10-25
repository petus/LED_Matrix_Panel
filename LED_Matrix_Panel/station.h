/**
 * @file    station.h
 * @brief   Decides which screen is shown.
 *
 * Order:
 *   1. WiFi set-up access point                  -> QR code + address
 *   2. connecting to the home WiFi (no time yet) -> "Pripojuji WiFi"
 *   3. first 10 s after connecting               -> IP address
 *   4. forced mode (web / console)               -> that screen
 *   5. night: display off, or clock only (settings)
 *   6. aircraft within the radius                -> aircraft
 *   7. rain within the radius                    -> radar animation
 *   8. otherwise                                 -> clock
 */
#pragma once

#include <stdint.h>

namespace Station {

  /// Auto     – by situation: clock (+ forecast), radar when it rains nearby,
  ///            aircraft at once / now and then / never (setting autoPlane)
  /// Rotate   – the chosen screens in turn, rotateS seconds each; aircraft
  ///            skipped when none is around, radar when it does not rain
  ///            (unless rotateRadarDry)
  /// Clock … Forecast – one fixed screen
  /// The mode is saved in the settings and survives a restart.
  enum class Mode : uint8_t { Auto, Clock, Plane, Radar, Forecast, Rotate };
  constexpr uint8_t MODE_COUNT = 6;

  /// Start shared data and the network task (WiFi is started separately).
  void begin();

  /// Call from loop() as often as possible – draws at FRAME_INTERVAL_MS.
  void update();

  void        setMode(Mode m);
  void        cycleMode();       // Auto -> Clock -> Plane -> Radar -> Forecast -> Rotate -> Auto
  Mode        mode();
  const char* modeName(Mode m);
  const char* screenNameNow();
  bool        nightNow();
  void        refreshRadar();
  void        printInfo();

}  // namespace Station
