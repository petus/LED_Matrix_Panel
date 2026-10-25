/**
 * @file    time_sync.h
 * @brief   Clock set from the "Date" header of ordinary HTTP responses.
 *
 * Every web server sends the current UTC time in its response header:
 *   Date: Thu, 22 Oct 2026 19:21:05 GMT
 * The station talks to adsb.fi every few seconds anyway, so this keeps the
 * clock accurate to about a second without NTP. The ESP32 RTC keeps counting
 * between the updates.
 */
#pragma once

#include <time.h>
#include <stdint.h>

namespace TimeSync {

  /// Set the local time zone (call once at start-up).
  void begin(const char* posixTz);

  /// Parse an RFC 7231 date ("Thu, 22 Oct 2026 19:21:05 GMT") and set the clock.
  /// Returns false when the text could not be parsed.
  bool setFromHttpDate(const char* date);

  /// Current local time. Returns false while the clock is not valid yet.
  /// ms (optional) receives the milliseconds of the current second.
  bool now(struct tm* local, uint16_t* ms = nullptr);

}  // namespace TimeSync
