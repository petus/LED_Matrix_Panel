/**
 * @file    chmu.h
 * @brief   ČHMÚ precipitation radar composite (opendata.chmi.cz).
 *
 * Source: https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/
 *   - one PNG every 5 minutes, maximum reflectivity (MAX Z) of the Czech
 *     radar network, Web Mercator, 1 km resolution
 *   - file names carry the UTC time: pacz2gmaps3.z_max3d.YYYYMMDD.HHMM.*.png
 *
 * The PNG is decoded line by line (PNGdec) and reduced straight into a
 * 96x48 radar grid in the panel projection (radar_map.h). Each panel pixel
 * keeps the strongest echo of all source pixels it covers, so small storm
 * cells do not disappear. The colours are converted back to levels
 * 1–15 (4–60 dBZ) by the nearest colour of the ČHMÚ scale.
 */
#pragma once

#include <stdint.h>
#include "radar_palette.h"

struct RainInfo {
  bool    nearby     = false;   // echo above the dBZ threshold within the rain radius
  float   nearestKm  = 0;       // distance of the closest such echo
  uint8_t maxLevel   = 0;       // strongest level within the radius
  char    timeUtc[13] = "";     // newest frame, "YYYYMMDDHHMM"
};

namespace CHMU {

  /// Allocate the PNG decoder (~45 kB). Call early, before WiFi starts,
  /// while the heap still has large free blocks.
  bool begin();

  /// Download the newest frames (oldest first) into grids.
  /// grids must hold maxFrames * RadarGrid::BYTES bytes.
  /// Returns the number of frames decoded (0 = failure, grids untouched
  /// beyond the returned count). info describes the newest frame.
  uint8_t fetch(uint8_t* grids, uint8_t maxFrames, double homeLat, double homeLon,
                float radiusKm, uint8_t minLevel, RainInfo* info);

}  // namespace CHMU
