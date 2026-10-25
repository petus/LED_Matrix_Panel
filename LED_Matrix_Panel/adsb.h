/**
 * @file    adsb.h
 * @brief   Nearest aircraft from the free adsb.fi API.
 *
 * API: https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/{nautical miles}
 * Only airborne aircraft are taken. An aircraft counts when it is
 *   - within the radius around home (any direction), or
 *   - in the viewing sector (optional): bearing within ±half-width of the
 *     viewing direction and not farther than the viewing range.
 * Shown is the nearest aircraft in the viewing sector; when there is none,
 * the nearest one within the radius.
 */
#pragma once

#include <stdint.h>

struct Aircraft {
  char  callsign[10] = "";   // flight number, e.g. "CSA123" (may be empty)
  char  reg[12]      = "";   // registration, e.g. "OK-TVU"
  char  type[8]      = "";   // ICAO type, e.g. "A320"
  char  hex[8]       = "";   // ICAO 24-bit address, always present
  float lat          = 0;
  float lon          = 0;
  float altM         = 0;    // barometric altitude in metres
  float speedKmh     = 0;    // ground speed in km/h
  float trackDeg     = 0;    // ground track, 0 = north
  bool  hasTrack     = false;
  float distKm       = 0;    // distance from HOME
  float bearingDeg   = 0;    // direction from HOME, 0 = north, 90 = east
  bool  inView       = false;// inside the viewing sector
};

namespace ADSB {

  /// Which aircraft count (from the web settings).
  struct Filter {
    float    radiusKm     = 10;
    uint16_t minAltM      = 0;       // ignore lower ones (e.g. at an airport)
    bool     callsignOnly = false;   // ignore aircraft without a flight number
    // Viewing sector (e.g. the view from a window)
    bool     viewEnabled  = false;
    uint16_t viewDirDeg   = 135;     // 0 = north, 90 = east, 135 = south-east ...
    uint8_t  viewHalfDeg  = 30;      // sector = viewDir ± viewHalf
    float    viewRangeKm  = 5;
  };

  /// Bearing from point 1 to point 2 in degrees, 0..360 (0 = north, clockwise).
  float bearingDeg(double lat1, double lon1, double lat2, double lon2);

  /// True when bearing b lies within ±halfWidth of direction dir (all degrees).
  bool inSector(float b, float dir, float halfWidth);

  /// Ask adsb.fi for aircraft around (lat, lon).
  /// Result of one query.
  struct Result {
    bool     found = false;   // an airborne aircraft passes the filter
    Aircraft plane;           // the nearest one
    uint8_t  count = 0;       // how many aircraft pass the filter (incl. plane)
  };

  /// Aircraft around (lat, lon): the nearest one that passes the filter
  /// and how many pass it in total. Returns false on network / parse error.
  bool fetch(double lat, double lon, const Filter& filter, Result* out);

}  // namespace ADSB
