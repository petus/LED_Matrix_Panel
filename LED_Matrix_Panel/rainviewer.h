/**
 * @file    rainviewer.h
 * @brief   RainViewer precipitation radar (api.rainviewer.com) as a second
 *          source next to ČHMÚ.
 *
 * RainViewer publishes the frames of the last two hours (one every 10 min)
 * as standard Web Mercator tiles. The panel map uses Web Mercator too, so a
 * tile pixel lands on the panel by a simple linear formula. At zoom 6 the
 * whole map (Czech Republic + surroundings) is covered by 2x2 tiles, about
 * four tile pixels per panel pixel.
 *
 * All tiles of one frame share one kept-alive HTTPS connection.
 *
 * Every tile is decoded line by line while it downloads (PNGdec + the same
 * sliding window as the ČHMÚ images) straight into the 4-bit radar grid,
 * each panel pixel keeping the strongest echo. RainViewer colours
 * ("Universal Blue", dBZ table from rainviewer.com) are converted back to dBZ
 * and stored as the same levels as ČHMÚ, so the map, the legend and the rain
 * threshold work the same for both sources.
 *
 * Free for personal / non-commercial use, no key needed. Max. zoom 7.
 */
#pragma once

#include <stdint.h>
#include "chmu.h"

namespace RainViewer {

  /// Download the newest frames (oldest first) into grids, same contract as
  /// CHMU::fetch(). known = "YYYYMMDDHHMM" (UTC) of the newest frame already
  /// shown, or nullptr; returns RADAR_UNCHANGED when there is nothing new.
  /// idle (optional) is called between frames, with no connection open.
  uint8_t fetch(uint8_t* grids, uint8_t maxFrames, double homeLat, double homeLon,
                float radiusKm, uint8_t minLevel, RainInfo* info, const char* known,
                RadarIdleFn idle = nullptr);

  /// RainViewer colour (RGB565, blended against black) -> level 0..15.
  /// Exposed for the host tests.
  uint8_t colorToLevel(uint16_t c);

}  // namespace RainViewer
