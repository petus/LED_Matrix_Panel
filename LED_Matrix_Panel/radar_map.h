/**
 * @file    radar_map.h
 * @brief   Map of the Czech Republic rasterised for the 96x48 panel,
 *          plus the projection used to place anything on it.
 *
 * Generated from Natural Earth 1:10m country borders (public domain),
 * Web Mercator projection (the same one the ČHMÚ radar PNGs use),
 * map height 46 px, offset 1 px from the top-left corner.
 *
 * Characters in ROWS:
 *   '.'  outside the Czech Republic
 *   ':'  inside
 *   '#'  border (inside pixel next to the outside)
 */
#pragma once

#include <stdint.h>

namespace RadarMap {

  constexpr int16_t WIDTH  = 96;
  constexpr int16_t HEIGHT = 48;

  /// One string per row, WIDTH characters each.
  extern const char* const ROWS[HEIGHT];

  /// Map character at (x, y), '.' when outside the map array.
  inline char at(int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return '.';
    return ROWS[y][x];
  }

  // ---------------------------------------------------------------------------
  //  Projection – must match the generator that produced ROWS
  //    px = OFFSET_X + (lon_rad - LON_MIN_RAD) * SCALE
  //    py = OFFSET_Y + (MERC_Y_MAX - mercY(lat)) * SCALE
  //  Pixel (x, y) covers [x, x+1) x [y, y+1).
  // ---------------------------------------------------------------------------
  constexpr float SCALE       = 685.18815f;    // px per radian
  constexpr float LON_MIN_RAD = 0.21076842f;   // 12.076141° E (west tip of CZ)
  constexpr float MERC_Y_MAX  = 1.03923363f;   // 51.040012° N (north tip of CZ)
  constexpr float OFFSET_X    = 1.0f;
  constexpr float OFFSET_Y    = 1.0f;

  /// Geographic position -> panel pixel (may be outside the panel).
  void project(float latDeg, float lonDeg, int16_t* x, int16_t* y);

  /// Same, with sub-pixel precision.
  void projectF(float latDeg, float lonDeg, float* x, float* y);

}  // namespace RadarMap
