/**
 * @file    adsb.cpp
 * @brief   adsb.fi client (see adsb.h). Uses ArduinoJson 7.
 */
#include "adsb.h"
#include "net.h"
#include "debug.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace {

constexpr const char* API_BASE   = "https://opendata.adsb.fi/api/v3/lat/";
constexpr float       KM_PER_NM  = 1.852f;
constexpr float       M_PER_FT   = 0.3048f;
constexpr float       KMH_PER_KT = 1.852f;
constexpr size_t      MAX_BODY   = 32 * 1024;   // 10 km radius -> a few kB

// Copy a string field, trimming the spaces adsb.fi pads callsigns with.
void copyTrimmed(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  while (*src == ' ') src++;
  size_t n = 0;
  while (src[n] && n < cap - 1) { dst[n] = src[n]; n++; }
  while (n > 0 && dst[n - 1] == ' ') n--;
  dst[n] = '\0';
}

// A number that may also arrive as a string ("alt_baro" is "ground" on the ground).
bool readNumber(JsonVariantConst v, float* out) {
  if (v.is<float>() || v.is<int>()) { *out = v.as<float>(); return true; }
  return false;
}

// Great-circle distance in km.
float distanceKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  const double dLat = (lat2 - lat1) * DEG_TO_RAD;
  const double dLon = (lon2 - lon1) * DEG_TO_RAD;
  const double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
                   sin(dLon / 2) * sin(dLon / 2);
  return (float)(2 * R * atan2(sqrt(a), sqrt(1 - a)));
}

}  // namespace

namespace ADSB {

float bearingDeg(double lat1, double lon1, double lat2, double lon2) {
  // Initial great-circle bearing. atan2(east, north) -> 0 = north, 90 = east.
  const double p1 = lat1 * DEG_TO_RAD, p2 = lat2 * DEG_TO_RAD;
  const double dl = (lon2 - lon1) * DEG_TO_RAD;
  const double east  = sin(dl) * cos(p2);
  const double north = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
  double b = atan2(east, north) * RAD_TO_DEG;
  if (b < 0) b += 360.0;
  return (float)b;
}

bool inSector(float b, float dir, float halfWidth) {
  // Signed difference folded into -180..180, so 350° vs 10° gives 20°, not 340°.
  float diff = fmodf(b - dir, 360.0f);
  if (diff > 180.0f)  diff -= 360.0f;
  if (diff < -180.0f) diff += 360.0f;
  return fabsf(diff) <= halfWidth;
}

bool fetch(double lat, double lon, const Filter& filter, Result* out) {
  const float radiusKm = filter.radiusKm;
  const bool  view     = filter.viewEnabled;
  // adsb.fi answers for a circle – big enough for both the radius and the view.
  const float queryKm  = (view && filter.viewRangeKm > radiusKm) ? filter.viewRangeKm : radiusKm;
  char url[128];
  snprintf(url, sizeof(url), "%s%.5f/lon/%.5f/dist/%.1f",
           API_BASE, lat, lon, queryKm / KM_PER_NM);

  uint8_t* body = nullptr;
  const long len = Net::getToBuffer(url, &body, MAX_BODY, "ADSB");
  if (len <= 0) return false;

  // Keep only the fields we use – the document stays small.
  JsonDocument fields;
  JsonObject f = fields["ac"].add<JsonObject>();
  for (const char* key : { "hex", "flight", "r", "t", "lat", "lon",
                           "alt_baro", "gs", "track", "true_heading" }) {
    f[key] = true;
  }

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, (const char*)body, (size_t)len, DeserializationOption::Filter(fields));
  free(body);
  if (err) {
    DEBUG_LOG("ADSB", "JSON error: %s", err.c_str());
    return false;
  }

  JsonArrayConst list = doc["ac"].as<JsonArrayConst>();
  int      total = 0;
  uint8_t  count = 0;
  float    bestNear = 1e9f, bestView = 1e9f;
  bool     haveNear = false, haveView = false;
  Aircraft nearest, viewed;                 // nearest within radius / in the view

  for (JsonObjectConst p : list) {
    total++;
    float plat, plon, alt;
    if (!readNumber(p["lat"], &plat) || !readNumber(p["lon"], &plon)) continue;
    if (!readNumber(p["alt_baro"], &alt)) continue;          // "ground" -> skip

    const float d = distanceKm(lat, lon, plat, plon);
    const float b = bearingDeg(lat, lon, plat, plon);
    const bool  near   = d <= radiusKm;
    const bool  inView = view && d <= filter.viewRangeKm &&
                         inSector(b, filter.viewDirDeg, filter.viewHalfDeg);
    if (!near && !inView) continue;
    if (alt * M_PER_FT < filter.minAltM) continue;

    Aircraft a;
    copyTrimmed(a.callsign, sizeof(a.callsign), p["flight"] | "");
    if (filter.callsignOnly && !a.callsign[0]) continue;
    copyTrimmed(a.hex, sizeof(a.hex), p["hex"] | "");
    if (count < 255) count++;

    const bool bestInView = inView && d < bestView;
    const bool bestNearby = near && d < bestNear;
    if (!bestInView && !bestNearby) continue;               // counted only

    copyTrimmed(a.reg,  sizeof(a.reg),  p["r"] | "");
    copyTrimmed(a.type, sizeof(a.type), p["t"] | "");
    a.lat    = plat;
    a.lon    = plon;
    a.altM   = alt * M_PER_FT;
    a.distKm = d;
    float v;
    if (readNumber(p["gs"], &v)) a.speedKmh = v * KMH_PER_KT;
    if (readNumber(p["track"], &v) || readNumber(p["true_heading"], &v)) {
      a.trackDeg = v;
      a.hasTrack = true;
    }
    a.bearingDeg = b;
    a.inView     = inView;
    if (bestInView) { bestView = d; viewed  = a; haveView = true; }
    if (bestNearby) { bestNear = d; nearest = a; haveNear = true; }
  }

  // The view has priority: that is the aircraft you are looking at.
  out->found = haveView || haveNear;
  out->count = count;
  if (out->found) out->plane = haveView ? viewed : nearest;

  if (out->found) {
    DEBUG_LOG("ADSB", "%d aircraft, %u counted, %s %s %s %.1f km at %.0f deg, %.0f m, %.0f km/h",
              total, (unsigned)count, out->plane.inView ? "IN VIEW" : "nearest",
              out->plane.callsign[0] ? out->plane.callsign : out->plane.hex,
              out->plane.type, out->plane.distKm, out->plane.bearingDeg,
              out->plane.altM, out->plane.speedKmh);
  } else {
    DEBUG_LOG("ADSB", "%d aircraft, none airborne within %.0f km%s", total, radiusKm,
              view ? " or in the view" : "");
  }
  return true;
}

}  // namespace ADSB
