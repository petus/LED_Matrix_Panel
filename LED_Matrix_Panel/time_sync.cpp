/**
 * @file    time_sync.cpp
 * @brief   HTTP "Date" header -> system clock.
 */
#include "time_sync.h"
#include "debug.h"

#include <Arduino.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace {

volatile bool s_valid = false;

int monthFromName(const char* m) {
  static const char NAMES[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  for (int i = 0; i < 12; i++) {
    if (strncmp(m, NAMES + i * 3, 3) == 0) return i + 1;
  }
  return 0;
}

// Days from 1970-01-01 to the given civil date (proleptic Gregorian).
// Avoids timegm(), which is not available everywhere.
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

}  // namespace

namespace TimeSync {

void begin(const char* posixTz) {
  setenv("TZ", posixTz, 1);
  tzset();
  DEBUG_LOG("TIME", "Time zone %s", posixTz);
}

bool setFromHttpDate(const char* date) {
  if (!date) return false;
  const char* p = strchr(date, ' ');           // skip "Thu,"
  if (!p) return false;

  int day, year, hh, mm, ss;
  char mon[4] = {0};
  if (sscanf(p + 1, "%d %3s %d %d:%d:%d", &day, mon, &year, &hh, &mm, &ss) != 6) return false;
  const int month = monthFromName(mon);
  if (!month || year < 2024 || day < 1 || day > 31 || hh > 23 || mm > 59 || ss > 60) return false;

  const time_t utc = (time_t)daysFromCivil(year, month, day) * 86400L + hh * 3600L + mm * 60L + ss;

  // The header has whole seconds only. Re-setting the clock on every response
  // would make the gliding second hand jump back and forth, so a running clock
  // is corrected only when it is really off.
  if (s_valid) {
    const long diff = (long)(time(nullptr) - utc);
    if (diff >= -1 && diff <= 1) return true;
    DEBUG_LOG("TIME", "Clock corrected by %ld s", -diff);
  }
  struct timeval tv = { utc, 0 };
  settimeofday(&tv, nullptr);

  if (!s_valid) {
    s_valid = true;
    struct tm lt;
    localtime_r(&utc, &lt);
    DEBUG_LOG("TIME", "Clock set from HTTP Date: %02d.%02d.%04d %02d:%02d:%02d local",
              lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900, lt.tm_hour, lt.tm_min, lt.tm_sec);
  }
  return true;
}

bool now(struct tm* local, uint16_t* ms) {
  if (!s_valid) return false;
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  const time_t t = tv.tv_sec;
  localtime_r(&t, local);
  if (ms) *ms = (uint16_t)(tv.tv_usec / 1000);
  return true;
}

}  // namespace TimeSync
