/**
 * @file    weather_icons.h
 * @brief   16x16 weather icons drawn with Adafruit GFX primitives.
 */
#pragma once

#include <stdint.h>
#include "forecast.h"

namespace WeatherIcon {

  constexpr int16_t SIZE = 16;

  /// Draw the icon with its top-left corner at (x, y).
  /// day = false shows a moon instead of the sun (clear / partly cloudy).
  void draw(int16_t x, int16_t y, Weather::Kind kind, bool day);

}  // namespace WeatherIcon
