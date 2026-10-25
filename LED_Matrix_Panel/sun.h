/**
 * @file    sun.h
 * @brief   Sunrise and sunset times (NOAA approximation, about ±2 minutes).
 *          Used for the night mode "from sunset to sunrise".
 */
#pragma once

#include <stdint.h>

namespace Sun {

  /// Sunrise and sunset in minutes after local midnight for the given date.
  /// utcOffsetMin = local time minus UTC (e.g. +120 in Central European summer).
  /// Returns false for polar day / night.
  bool times(int year, int month, int day, double lat, double lon,
             int utcOffsetMin, int* sunriseMin, int* sunsetMin);

}  // namespace Sun
