/**
 * @file    sun.cpp
 * @brief   NOAA sunrise / sunset equations (see sun.h).
 */
#include "sun.h"

#include <math.h>

namespace {

constexpr double DEG = 0.017453292519943295;

int dayOfYear(int y, int m, int d) {
  static const int CUM[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  const bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
  return CUM[m - 1] + d + ((leap && m > 2) ? 1 : 0);
}

}  // namespace

namespace Sun {

bool times(int year, int month, int day, double lat, double lon,
           int utcOffsetMin, int* sunriseMin, int* sunsetMin) {
  const int    n     = dayOfYear(year, month, day);
  const double gamma = 2.0 * M_PI / 365.0 * (n - 1);    // fractional year

  // Equation of time (minutes) and solar declination (radians)
  const double eqTime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma)
                                  - 0.014615 * cos(2 * gamma) - 0.040849 * sin(2 * gamma));
  const double decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma)
                    - 0.006758 * cos(2 * gamma) + 0.000907 * sin(2 * gamma)
                    - 0.002697 * cos(3 * gamma) + 0.00148 * sin(3 * gamma);

  // Hour angle for the sun 0.833° below the horizon (refraction + disc size)
  const double cosHa = cos(90.833 * DEG) / (cos(lat * DEG) * cos(decl))
                     - tan(lat * DEG) * tan(decl);
  if (cosHa > 1 || cosHa < -1) return false;
  const double ha = acos(cosHa) / DEG;

  const double noonUtc = 720.0 - 4.0 * lon - eqTime;    // minutes after UTC midnight
  *sunriseMin = (int)lround(noonUtc - 4.0 * ha + utcOffsetMin);
  *sunsetMin  = (int)lround(noonUtc + 4.0 * ha + utcOffsetMin);
  return true;
}

}  // namespace Sun
