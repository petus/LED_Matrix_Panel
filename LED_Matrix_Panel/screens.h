/**
 * @file    screens.h
 * @brief   The three screens of the sky station. Each function draws one
 *          complete frame into the (already cleared) back buffer.
 */
#pragma once

#include <stdint.h>
#include <time.h>
#include "adsb.h"
#include "settings.h"
#include "forecast.h"

namespace Screens {

  /// Language of the texts on the panel (false = Czech).
  void setEnglish(bool en);

  /// Clock – analog (anti-aliased hands, gliding second hand) or digital,
  /// colours and options from the settings. ms = milliseconds of the second.
  /// local == nullptr -> time not known yet (shows "--:--").
  void clock(const struct tm* local, uint16_t ms, const Settings::Values& cfg);

  /// Aircraft: rotated plane icon on the left, details on the right.
  /// a == nullptr -> "no aircraft" note (forced mode without a plane).
  /// inRange > 1 adds "+N" for the other aircraft in range.
  void plane(const Aircraft* a, uint8_t inRange);

  /// Radar map with one animation frame, digital clock in the free corner.
  /// grid == nullptr -> map only.
  void radar(const uint8_t* grid, uint8_t frame, uint8_t frames, const struct tm* local,
             const Settings::Values& cfg);

  /// Weather in three columns: now, today, tomorrow (f == nullptr -> waiting).
  void forecast(const Weather::Forecast* f);

  /// First set-up: QR code to join the access point, its name and address.
  void setupAp(const char* ssid, const char* ip);

  /// Waiting for the home WiFi (animated dots, ms = time since start).
  void connecting(const char* ssid, uint32_t ms);

  /// Address of the panel in the home network (shown 10 s after connecting).
  void ipAddress(const char* ip, const char* hostname, const char* ssid);

}  // namespace Screens
