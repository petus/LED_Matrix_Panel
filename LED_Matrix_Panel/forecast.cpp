/**
 * @file    forecast.cpp
 * @brief   Open-Meteo client (see forecast.h). Uses ArduinoJson 7.
 */
#include "forecast.h"
#include "net.h"
#include "debug.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

constexpr size_t MAX_BODY = 8 * 1024;    // the answer has ~1 kB

}  // namespace

namespace Weather {

Kind kind(uint8_t c) {
  if (c == 0 || c == 1)               return Kind::Clear;          // clear, mainly clear
  if (c == 2)                         return Kind::PartlyCloudy;
  if (c == 3)                         return Kind::Cloudy;         // overcast
  if (c == 45 || c == 48)             return Kind::Fog;
  if (c >= 51 && c <= 57)             return Kind::Drizzle;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return Kind::Rain;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86)   return Kind::Snow;
  if (c >= 95)                        return Kind::Storm;
  return Kind::Cloudy;
}

bool fetch(double lat, double lon, Forecast* out) {
  char url[400];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,wind_speed_10m,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&wind_speed_unit=ms&timezone=auto&forecast_days=2",
           lat, lon);

  uint8_t* body = nullptr;
  const long len = Net::getToBuffer(url, &body, MAX_BODY, "METEO");
  if (len <= 0) return false;

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, (const char*)body, (size_t)len);
  free(body);
  if (err) {
    DEBUG_LOG("METEO", "JSON error: %s", err.c_str());
    return false;
  }

  JsonObjectConst cur = doc["current"];
  JsonObjectConst day = doc["daily"];
  if (cur.isNull() || day.isNull() || day["temperature_2m_max"].size() < 2) {
    DEBUG_LOG("METEO", "Unexpected answer");
    return false;
  }

  Forecast f;
  f.temp   = cur["temperature_2m"] | 0.0f;
  f.code   = cur["weather_code"]   | 0;
  f.windMs = cur["wind_speed_10m"] | 0.0f;
  f.isDay  = (cur["is_day"] | 1) != 0;
  for (uint8_t i = 0; i < 2; i++) {
    f.day[i].code     = day["weather_code"][i]                  | 0;
    f.day[i].tMax     = day["temperature_2m_max"][i]            | 0.0f;
    f.day[i].tMin     = day["temperature_2m_min"][i]            | 0.0f;
    f.day[i].rainProb = day["precipitation_probability_max"][i] | 0;
  }
  f.valid = true;
  *out = f;

  DEBUG_LOG("METEO", "Now %.1f C code %u, today %.0f/%.0f C code %u, tomorrow %.0f/%.0f C code %u",
            f.temp, f.code, f.day[0].tMax, f.day[0].tMin, f.day[0].code,
            f.day[1].tMax, f.day[1].tMin, f.day[1].code);
  return true;
}

}  // namespace Weather
