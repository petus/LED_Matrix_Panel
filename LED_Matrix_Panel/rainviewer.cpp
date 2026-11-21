/**
 * @file    rainviewer.cpp
 * @brief   RainViewer tiles -> panel radar grids (see rainviewer.h).
 */
#include "rainviewer.h"
#include "config.h"
#include "debug.h"
#include "net.h"
#include "radar_map.h"
#include "radar_palette.h"
#include "png_stream.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PNGdec.h>
#include <math.h>
#include <string.h>
#include <time.h>

namespace {

constexpr size_t   MAX_INDEX  = 16 * 1024;     // weather-maps.json is a few kB
constexpr uint8_t  MAX_FRAMES = 12;
constexpr size_t   PATH_LEN   = 48;
constexpr double   PI_D       = 3.14159265358979323846;

// "Universal Blue" (colour scheme 2) rain colours, RGBA, for 4..64 dBZ.
// Source: https://www.rainviewer.com/files/rainviewer_api_colors_table.csv
// 65 dBZ and more is white. Below 4 dBZ nothing is stored (level 0).
constexpr int8_t   RV_DBZ_FIRST = 4;
constexpr uint32_t RV_RAIN[] = {
  0x8e856f5e, 0x92887164, 0x9e93756e, 0xaa9e7978, 0xb6a97e82, 0xc2b4828c,   //  4.. 9
  0xcec08796, 0xd2c48ba0, 0xd6c88faa, 0xdacc93b4, 0xded097be,               // 10..14
  0x88ddeeff, 0x6cd1ebff, 0x51c5e8ff, 0x36bae5ff, 0x1baee2ff,               // 15..19
  0x00a3e0ff, 0x009ad5ff, 0x0091caff, 0x0088bfff, 0x007fb4ff,               // 20..24
  0x0077aaff, 0x0070a3ff, 0x00699cff, 0x006295ff, 0x005b8eff,               // 25..29
  0x005588ff, 0x005180ff, 0x004e78ff, 0x004a70ff, 0x004768ff,               // 30..34
  0xffee00ff, 0xffe000ff, 0xffd200ff, 0xffc500ff, 0xffb700ff,               // 35..39
  0xffaa00ff, 0xff9f00ff, 0xff9500ff, 0xff8b00ff, 0xff8100ff,               // 40..44
  0xff4400ff, 0xf23600ff, 0xe62800ff, 0xd91b00ff, 0xcd0d00ff,               // 45..49
  0xc10000ff, 0xa80000ff, 0x8f0000ff, 0x760000ff, 0x5d0000ff,               // 50..54
  0xffaaffff, 0xff9fffff, 0xff95ffff, 0xff8bffff, 0xff81ffff,               // 55..59
  0xff77ffff, 0xff6cffff, 0xff62ffff, 0xff58ffff, 0xff4effff,               // 60..64
  0xffffffff,                                                               // 65+
};
constexpr uint8_t RV_COUNT = sizeof(RV_RAIN) / sizeof(RV_RAIN[0]);

// Same tolerance as for the ČHMÚ colours: anything further away from every
// scale colour (borders of the transparent area, rounding) is not rain.
constexpr int32_t MAX_COLOR_DIST2 = 24 * 24 * 3;

// The scale blended against black, as PNGdec delivers it (RGB 8-bit).
uint8_t s_scale[RV_COUNT][3];
bool    s_scaleReady = false;

void prepareScale() {
  if (s_scaleReady) return;
  for (uint8_t i = 0; i < RV_COUNT; i++) {
    const uint32_t c = RV_RAIN[i];
    const uint32_t a = c & 0xFF;
    s_scale[i][0] = (uint8_t)(((c >> 24) & 0xFF) * a / 255);
    s_scale[i][1] = (uint8_t)(((c >> 16) & 0xFF) * a / 255);
    s_scale[i][2] = (uint8_t)(((c >> 8)  & 0xFF) * a / 255);
  }
  s_scaleReady = true;
}

// Web Mercator, world pixels at RV_ZOOM
constexpr double WORLD = (double)RV_TILE_SIZE * (1 << RV_ZOOM);

// Panel pixel of a world pixel (linear – both are Web Mercator).
inline float worldToPanelX(double wx) {
  return RadarMap::OFFSET_X +
         (float)((2.0 * PI_D * wx / WORLD - PI_D - RadarMap::LON_MIN_RAD) * RadarMap::SCALE);
}
inline float worldToPanelY(double wy) {
  return RadarMap::OFFSET_Y +
         (float)((RadarMap::MERC_Y_MAX - PI_D + 2.0 * PI_D * wy / WORLD) * RadarMap::SCALE);
}
// And back (for the tile range that covers the panel)
inline double panelToWorldX(float px) {
  return ((px - RadarMap::OFFSET_X) / RadarMap::SCALE + RadarMap::LON_MIN_RAD + PI_D) * WORLD / (2.0 * PI_D);
}
inline double panelToWorldY(float py) {
  return ((py - RadarMap::OFFSET_Y) / RadarMap::SCALE - RadarMap::MERC_Y_MAX + PI_D) * WORLD / (2.0 * PI_D);
}
inline double worldToLon(double wx) { return wx / WORLD * 360.0 - 180.0; }
inline double worldToLat(double wy) {
  const double n = PI_D * (1.0 - 2.0 * wy / WORLD);
  return atan(sinh(n)) * 180.0 / PI_D;
}

// -----------------------------------------------------------------------------
//  Tile decoding
// -----------------------------------------------------------------------------
struct TileCtx {
  PNG*      png;
  uint8_t*  grid;
  uint16_t* line;
  int16_t   colPx[RV_TILE_SIZE];   // panel x for each tile column (-1 = off the panel)
  float     colKm[RV_TILE_SIZE];   // east-west km from home
  int       tx, ty;                // tile numbers
  bool      detect;
  float     homeLat;
  float     radiusKm;
  uint8_t   minLevel;
  RainInfo* info;
  uint16_t  lastColor;
  uint8_t   lastLevel;
  uint32_t  rainPx;
};

int onTileLine(PNGDRAW* d) {
  TileCtx& c = *(TileCtx*)d->pUser;
  if (d->iWidth > RV_TILE_SIZE) return 0;                  // not what we asked for
  const double wy = (double)c.ty * RV_TILE_SIZE + d->y + 0.5;
  const int16_t py = (int16_t)floorf(worldToPanelY(wy));
  const bool inPanel = (py >= 0 && py < RadarMap::HEIGHT);
  if (!inPanel && !c.detect) return 1;

  c.png->getLineAsRGB565(d, c.line, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  const float kmY = ((float)worldToLat(wy) - c.homeLat) * 111.32f;

  for (int sx = 0; sx < d->iWidth; sx++) {
    const uint16_t col = c.line[sx];
    if (col == 0) continue;
    uint8_t level;
    if (col == c.lastColor) level = c.lastLevel;
    else { level = RainViewer::colorToLevel(col); c.lastColor = col; c.lastLevel = level; }
    if (!level) continue;
    c.rainPx++;

    const int16_t px = c.colPx[sx];
    if (inPanel && px >= 0) RadarGrid::setMax(c.grid, px, py, level);

    if (c.detect && level >= c.minLevel) {
      const float kmX  = c.colKm[sx];
      const float dist = sqrtf(kmX * kmX + kmY * kmY);
      if (dist <= c.radiusKm) {
        if (!c.info->nearby || dist < c.info->nearestKm) c.info->nearestKm = dist;
        c.info->nearby = true;
        if (level > c.info->maxLevel) c.info->maxLevel = level;
      }
    }
  }
  return 1;
}

TileCtx* s_ctx = nullptr;               // ~1.5 kB, allocated for one fetch

struct TileJob {
  long size    = 0;
  bool started = false;
  bool ok      = false;
};

bool decodeTile(Net::ReadFn read, void* stream, long size, void* user) {
  TileJob& j = *static_cast<TileJob*>(user);
  j.started = true;
  j.size    = size;
  PngStream::Source& src = PngStream::shared;
  src.reset(read, stream, (int32_t)size);
  PngStream::current = &src;

  TileCtx& c = *s_ctx;
  PNG* png = c.png;
  bool ok = false;
  if (png->open("", PngStream::openCb, PngStream::closeCb, PngStream::readCb,
                PngStream::seekCb, onTileLine) == PNG_SUCCESS) {
    const int h = png->getHeight();
    const int pitch = png->getBufferSize() / (h > 0 ? h : 1);
    if (png->getWidth() > RV_TILE_SIZE || 2 * (pitch + 1) > (int)PNG_MAX_BUFFERED_PIXELS) {
      DEBUG_LOG("RV", "Tile format not supported (%d x %d, bpp %d)", png->getWidth(), h, png->getBpp());
    } else {
      ok = (png->decode(&c, 0) == PNG_SUCCESS);
      if (!ok) DEBUG_LOG("RV", "PNG decode error %d", png->getLastError());
    }
    png->close();
  } else {
    DEBUG_LOG("RV", "PNG open error %d", png->getLastError());
  }
  PngStream::current = nullptr;
  if (src.error) DEBUG_LOG("RV", "Stream: data ended at %ld of %ld B", (long)src.errorPos, (long)src.size);
  j.ok = ok && !src.error;
  return j.ok;
}

// One tile with a few retries – a lost tile would leave a hole in the map.
bool fetchTile(const char* host, const char* path, int tx, int ty) {
  char url[200];
  snprintf(url, sizeof(url), "%s%s/%d/%d/%d/%d/%d/0_0.png", host, path, RV_TILE_SIZE, RV_ZOOM,
           tx, ty, RV_COLOR);
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    if (attempt) vTaskDelay(pdMS_TO_TICKS(500));
    TileJob job;
    if (Net::getStream(url, decodeTile, &job, "RV")) return true;
    if (!job.started) {
      // No Content-Length: download into RAM and decode from there.
      uint8_t* buf = nullptr;
      const long len = Net::getToBuffer(url, &buf, 96 * 1024, "RV");
      bool ok = false;
      if (len >= 8 && memcmp(buf, "\x89PNG", 4) == 0) {
        PNG* png = s_ctx->png;
        if (png->openRAM(buf, (int)len, onTileLine) == PNG_SUCCESS) {
          ok = (png->decode(s_ctx, 0) == PNG_SUCCESS);
          png->close();
        }
      }
      free(buf);
      if (ok) return true;
    }
  }
  DEBUG_LOG("RV", "Tile %d/%d failed", tx, ty);
  return false;
}

void stampFromUnix(time_t t, char out[13]) {
  struct tm u;
  gmtime_r(&t, &u);
  snprintf(out, 13, "%04d%02d%02d%02d%02d", u.tm_year + 1900, u.tm_mon + 1, u.tm_mday,
           u.tm_hour, u.tm_min);
}

}  // namespace

namespace RainViewer {

uint8_t colorToLevel(uint16_t c) {
  if (c == 0) return 0;
  prepareScale();
  const int r = (c >> 11) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  int32_t best = MAX_COLOR_DIST2 + 1;
  int8_t  idx  = -1;
  for (uint8_t i = 0; i < RV_COUNT; i++) {
    const int dr = r - s_scale[i][0], dg = g - s_scale[i][1], db = b - s_scale[i][2];
    const int32_t d = dr * dr + dg * dg + db * db;
    if (d < best) { best = d; idx = i; }
  }
  if (idx < 0) return 0;
  return RadarPalette::dbzToLevel((float)(RV_DBZ_FIRST + idx));
}

uint8_t fetch(uint8_t* grids, uint8_t maxFrames, double homeLat, double homeLon,
              float radiusKm, uint8_t minLevel, RainInfo* info, const char* known) {
  PNG* png = CHMU::decoder();
  if (!png) return 0;
  if (maxFrames > MAX_FRAMES) maxFrames = MAX_FRAMES;
  if (maxFrames > RADAR_FRAMES_MAX) maxFrames = RADAR_FRAMES_MAX;

  // 1) Index: host + the paths of the past frames (oldest first)
  uint8_t* body = nullptr;
  const long len = Net::getToBuffer(RV_INDEX_URL, &body, MAX_INDEX, "RV");
  if (len <= 0) { free(body); return 0; }

  JsonDocument filter;
  filter["host"] = true;
  JsonObject fp = filter["radar"]["past"].add<JsonObject>();
  fp["time"] = true;
  fp["path"] = true;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, (const char*)body, (size_t)len,
                                                   DeserializationOption::Filter(filter));
  free(body);
  if (err) { DEBUG_LOG("RV", "Index JSON: %s", err.c_str()); return 0; }

  char host[64];
  snprintf(host, sizeof(host), "%s", doc["host"] | "https://tilecache.rainviewer.com");
  JsonArrayConst past = doc["radar"]["past"];
  const int have = past.isNull() ? 0 : (int)past.size();
  if (have == 0) { DEBUG_LOG("RV", "Index has no frames"); return 0; }
  const int take  = have < maxFrames ? have : maxFrames;
  const int first = have - take;

  char newestStamp[13];
  stampFromUnix((time_t)(past[have - 1]["time"] | 0L), newestStamp);
  DEBUG_LOG("RV", "Index %ld B, %d frames, newest %s", len, have, newestStamp);
  if (known && strcmp(known, newestStamp) == 0) {
    DEBUG_LOG("RV", "No new frame");
    return RADAR_UNCHANGED;
  }

  char paths[RADAR_FRAMES_MAX][PATH_LEN];
  for (int i = 0; i < take; i++) {
    snprintf(paths[i], PATH_LEN, "%s", past[first + i]["path"] | "");
  }
  doc.clear();

  // 2) Tiles covering the panel map
  const int tx0 = (int)floor(panelToWorldX(0) / RV_TILE_SIZE);
  const int tx1 = (int)floor(panelToWorldX(RadarMap::WIDTH) / RV_TILE_SIZE);
  const int ty0 = (int)floor(panelToWorldY(0) / RV_TILE_SIZE);
  const int ty1 = (int)floor(panelToWorldY(RadarMap::HEIGHT) / RV_TILE_SIZE);

  s_ctx = (TileCtx*)malloc(sizeof(TileCtx));
  uint16_t* line = (uint16_t*)malloc(RV_TILE_SIZE * sizeof(uint16_t));
  if (!s_ctx || !line) {
    DEBUG_LOG("RV", "Out of memory for tile buffers");
    free(s_ctx); free(line); s_ctx = nullptr;
    return 0;
  }

  const float kmPerDegLon = 111.32f * cosf((float)homeLat * 0.017453293f);
  RainInfo newest;
  uint8_t done = 0;
  for (int f = 0; f < take; f++) {
    const bool isNewest = (f == take - 1);
    uint8_t* grid = grids + done * RadarGrid::BYTES;
    memset(grid, 0, RadarGrid::BYTES);
    bool ok = true;
    uint32_t rainPx = 0;
    for (int ty = ty0; ty <= ty1 && ok; ty++) {
      for (int tx = tx0; tx <= tx1 && ok; tx++) {
        TileCtx& c = *s_ctx;
        memset(&c, 0, sizeof(c));
        c.png = png; c.grid = grid; c.line = line; c.tx = tx; c.ty = ty;
        c.detect = isNewest; c.homeLat = (float)homeLat; c.radiusKm = radiusKm;
        c.minLevel = minLevel; c.info = &newest;
        for (int sx = 0; sx < RV_TILE_SIZE; sx++) {
          const double wx = (double)tx * RV_TILE_SIZE + sx + 0.5;
          const int16_t px = (int16_t)floorf(worldToPanelX(wx));
          c.colPx[sx] = (px >= 0 && px < RadarMap::WIDTH) ? px : -1;
          c.colKm[sx] = (float)(worldToLon(wx) - homeLon) * kmPerDegLon;
        }
        ok = fetchTile(host, paths[f], tx, ty);
        rainPx += c.rainPx;
      }
    }
    if (!ok) {
      DEBUG_LOG("RV", "Frame %d failed", f);
      break;
    }
    DEBUG_LOG("RV", "Frame %s: %d tiles, rain %lu px", paths[f], (tx1 - tx0 + 1) * (ty1 - ty0 + 1),
              (unsigned long)rainPx);
    done++;
  }
  free(line);
  free(s_ctx);
  s_ctx = nullptr;

  if (done == take) {
    memcpy(newest.timeUtc, newestStamp, 13);
    newest.source = RADAR_SRC_RAINVIEWER;
    *info = newest;
    if (newest.nearby) {
      DEBUG_LOG("RV", "Rain within %.0f km: nearest %.0f km, max %u dBZ",
                radiusKm, newest.nearestKm, RadarPalette::levelToDbz(newest.maxLevel));
    } else {
      DEBUG_LOG("RV", "No rain >= %u dBZ within %.0f km", RadarPalette::levelToDbz(minLevel), radiusKm);
    }
    return done;
  }
  return 0;
}

}  // namespace RainViewer
