/**
 * @file    forecast.h
 * @brief   Weather now, today and tomorrow from Open-Meteo (open-meteo.com).
 *
 * One small HTTPS request (about 1 kB of JSON), no API key. Weather is
 * described by the WMO code that Open-Meteo returns; Weather::kind() turns
 * it into one of a few icon types.
 */
#pragma once

#include <stdint.h>

namespace Weather {

  /// Icon type for a WMO weather code.
  enum class Kind : uint8_t { Clear, PartlyCloudy, Cloudy, Fog, Drizzle, Rain, Snow, Storm };

  Kind kind(uint8_t wmoCode);

  struct Day {
    uint8_t code     = 0;    // WMO weather code
    float   tMax     = 0;    // °C
    float   tMin     = 0;    // °C
    uint8_t rainProb = 0;    // highest precipitation probability, %
  };

  struct Forecast {
    bool    valid   = false;
    // Now
    float   temp    = 0;     // °C
    uint8_t code    = 0;     // WMO weather code
    float   windMs  = 0;     // m/s
    bool    isDay   = true;  // for the sun / moon icon
    // [0] today, [1] tomorrow
    Day     day[2];
  };

  /// Download the forecast for lat/lon. Returns false on network / parse error.
  bool fetch(double lat, double lon, Forecast* out);

}  // namespace Weather
