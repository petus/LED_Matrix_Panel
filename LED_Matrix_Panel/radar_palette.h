/**
 * @file    radar_palette.h
 * @brief   ČHMÚ reflectivity colour scale and a compact radar grid.
 *
 * Radar intensity is stored as a LEVEL 0–15:
 *   0      no precipitation
 *   1..15  4, 8, 12 ... 60 dBZ  (dBZ = level * 4)
 *
 * A grid covers the whole panel (96x48) with 4 bits per pixel,
 * i.e. 2304 bytes per radar frame.
 */
#pragma once

#include <stdint.h>
#include "colors.h"
#include "radar_map.h"

namespace RadarPalette {

  constexpr uint8_t LEVELS   = 15;   // 4..60 dBZ
  constexpr uint8_t DBZ_STEP = 4;

  /// RGB values of the ČHMÚ scale, index 0 = 4 dBZ ... 14 = 60 dBZ.
  constexpr uint8_t RGB[LEVELS][3] = {
    { 56,   0, 112}, { 48,   0, 168}, {  0,   0, 252}, {  0, 108, 192},
    {  0, 160,   0}, {  0, 188,   0}, { 52, 216,   0}, {156, 220,   0},
    {224, 220,   0}, {252, 176,   0}, {252, 132,   0}, {252,  88,   0},
    {252,   0,   0}, {160,   0,   0}, {252, 252, 252},
  };

  /// Panel colour for a level 1..15 (level 0 returns black).
  inline uint16_t color(uint8_t level) {
    if (level == 0) return 0;
    if (level > LEVELS) level = LEVELS;
    const uint8_t* c = RGB[level - 1];
    return rgb565(c[0], c[1], c[2]);
  }

  inline uint8_t levelToDbz(uint8_t level) { return level * DBZ_STEP; }

  inline uint8_t dbzToLevel(float dbz) {
    if (dbz < DBZ_STEP) return 0;
    int l = (int)(dbz / DBZ_STEP);
    return l > LEVELS ? LEVELS : (uint8_t)l;
  }

}  // namespace RadarPalette

// =============================================================================
//  Radar grid – one level (0–15) per panel pixel, 2 pixels per byte
// =============================================================================
namespace RadarGrid {

  constexpr uint16_t BYTES = RadarMap::WIDTH * RadarMap::HEIGHT / 2;   // 2304

  inline uint8_t get(const uint8_t* g, int16_t x, int16_t y) {
    const uint16_t i = y * RadarMap::WIDTH + x;
    return (i & 1) ? (g[i >> 1] >> 4) : (g[i >> 1] & 0x0F);
  }

  /// Store the level only if it is higher than the current one (block maximum).
  inline void setMax(uint8_t* g, int16_t x, int16_t y, uint8_t level) {
    const uint16_t i = y * RadarMap::WIDTH + x;
    uint8_t& b = g[i >> 1];
    if (i & 1) { if (level > (b >> 4))   b = (b & 0x0F) | (level << 4); }
    else       { if (level > (b & 0x0F)) b = (b & 0xF0) | level; }
  }

}  // namespace RadarGrid
