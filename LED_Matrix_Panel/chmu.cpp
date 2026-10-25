/**
 * @file    chmu.cpp
 * @brief   ČHMÚ radar download + decoding into panel grids (see chmu.h).
 *          Image geometry taken from the ČHMÚ data description and
 *          from MeteoPlaneRadar (github.com/petus/MeteoPlaneRadar).
 */
#include "chmu.h"
#include "net.h"
#include "debug.h"
#include "radar_map.h"
#include "png_stream.h"

#include <Arduino.h>
#include <PNGdec.h>
#include <math.h>
#include <string.h>
#include <new>

namespace {

// MAX Z composite, plain PNG (not the "png_masked" variant).
constexpr const char* INDEX_URL   = "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/";
constexpr const char* NAME_PREFIX = "pacz2gmaps3.z_max3d.";
constexpr size_t      MAX_PNG     = 128 * 1024;
constexpr uint8_t     MAX_FRAMES  = 12;
constexpr size_t      NAME_LEN    = 64;

// Geographic extent of the WHOLE PNG (ČHMÚ documentation), Web Mercator.
constexpr float IMG_LON_LEFT   = 11.267f;
constexpr float IMG_LON_RIGHT  = 20.770f;
constexpr float IMG_LAT_TOP    = 52.167f;
constexpr float IMG_LAT_BOTTOM = 48.047f;
// Radar data end here – right of / above it are the title and the colour scale.
constexpr float DATA_LON_RIGHT = 19.624f;
constexpr float DATA_LAT_TOP   = 51.458f;

// A source colour must be this close (squared RGB distance) to a scale colour,
// otherwise it is not rain (text, grey "no data" area, anti-aliasing).
constexpr int32_t MAX_COLOR_DIST2 = 40 * 40 * 3;

// PNGdec keeps its whole state, including a 32 kB zlib window, inside the PNG
// object (~45 kB). It is allocated ONCE at start-up – later, after WiFi and TLS
// have fragmented the heap, a single block that large is often not available.
PNG* s_png = nullptr;

float mercY(float latDeg) {
  return logf(tanf(0.785398163f + latDeg * 0.017453293f * 0.5f));
}
float invMercLat(float my) {
  return (2.0f * atanf(expf(my)) - 1.570796327f) * 57.29577951f;
}

// -----------------------------------------------------------------------------
//  Directory index: keep the N newest file names (sorted by time stamp)
// -----------------------------------------------------------------------------
struct Index {
  char    name[MAX_FRAMES][NAME_LEN];
  char    stamp[MAX_FRAMES][13];          // YYYYMMDDHHMM
  uint8_t count = 0;
  uint8_t want  = 0;
};

void indexInsert(Index& ix, const char* name, const char* stamp) {
  for (uint8_t i = 0; i < ix.count; i++) {
    if (strcmp(ix.stamp[i], stamp) == 0) return;          // already have it
  }
  if (ix.count == ix.want) {
    if (strcmp(stamp, ix.stamp[0]) <= 0) return;          // older than all kept
    memmove(ix.name[0], ix.name[1], (ix.count - 1) * NAME_LEN);   // drop oldest
    memmove(ix.stamp[0], ix.stamp[1], (ix.count - 1) * 13);
    ix.count--;
  }
  uint8_t p = ix.count;                                   // insertion sort
  while (p > 0 && strcmp(ix.stamp[p - 1], stamp) > 0) {
    memcpy(ix.name[p], ix.name[p - 1], NAME_LEN);
    memcpy(ix.stamp[p], ix.stamp[p - 1], 13);
    p--;
  }
  snprintf(ix.name[p], NAME_LEN, "%s", name);
  memcpy(ix.stamp[p], stamp, 13);
  ix.count++;
}

// Called for every window of the HTML listing.
void scanIndex(const char* text, void* user) {
  Index& ix = *(Index*)user;
  const size_t prefixLen = strlen(NAME_PREFIX);
  const char* pos = text;
  while ((pos = strstr(pos, NAME_PREFIX)) != nullptr) {
    const char* end = strstr(pos, ".png");
    if (!end) break;                                       // cut by the window edge
    const size_t len = end + 4 - pos;
    // Expected: prefix + YYYYMMDD '.' HHMM ...
    const char* ts = pos + prefixLen;
    if (len < NAME_LEN && len > prefixLen + 13 && ts[8] == '.') {
      char stamp[13];
      memcpy(stamp, ts, 8);
      memcpy(stamp + 8, ts + 9, 4);
      stamp[12] = '\0';
      bool digits = true;
      for (int i = 0; i < 12; i++) digits &= (stamp[i] >= '0' && stamp[i] <= '9');
      if (digits) {
        char name[NAME_LEN];
        memcpy(name, pos, len);
        name[len] = '\0';
        indexInsert(ix, name, stamp);
      }
    }
    pos = end + 4;
  }
}

// -----------------------------------------------------------------------------
//  PNG decoding
// -----------------------------------------------------------------------------
struct DecodeCtx {
  PNG*      png;
  uint8_t*  grid;           // target 96x48 grid
  uint16_t* line;           // one decoded source line (RGB565)
  int16_t*  colPx;          // panel x for each source column (-1 = skip)
  float*    colKm;          // east-west km from home for each source column
  int       width, height;
  int       dataX1, dataY0; // data area limits in source pixels
  // Rain detection (newest frame only)
  bool      detect;
  float     homeLat;
  float     radiusKm;
  uint8_t   minLevel;
  RainInfo* info;
  uint16_t  lastColor;      // tiny cache – neighbouring pixels repeat a lot
  uint8_t   lastLevel;
  // Diagnostics: source pixels in the data area that were / were not rain
  uint32_t  rainPx;
  uint32_t  otherPx;        // colours off the scale: text, borders, anti-aliasing
};

uint8_t colorToLevel(uint16_t c) {
  if (c == 0) return 0;
  const int r = (c >> 11) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  int32_t best = MAX_COLOR_DIST2 + 1;
  uint8_t level = 0;
  for (uint8_t i = 0; i < RadarPalette::LEVELS; i++) {
    const int dr = r - RadarPalette::RGB[i][0];
    const int dg = g - RadarPalette::RGB[i][1];
    const int db = b - RadarPalette::RGB[i][2];
    const int32_t d = dr * dr + dg * dg + db * db;
    if (d < best) { best = d; level = i + 1; }
  }
  return level;
}

int onLine(PNGDRAW* d) {
  DecodeCtx& c = *(DecodeCtx*)d->pUser;
  const int sy = d->y;
  if (sy < c.dataY0) return 1;                             // title area

  // Latitude of this source row -> panel row
  const float myTop = mercY(IMG_LAT_TOP), myBot = mercY(IMG_LAT_BOTTOM);
  const float my    = myTop - (float)sy * (myTop - myBot) / (c.height - 1);
  const float lat   = invMercLat(my);
  float fx, fy;
  RadarMap::projectF(lat, IMG_LON_LEFT, &fx, &fy);
  const int16_t py  = (int16_t)floorf(fy);
  const float   kmY = (lat - c.homeLat) * 111.32f;
  const bool    inPanel = (py >= 0 && py < RadarMap::HEIGHT);
  if (!inPanel && !c.detect) return 1;

  c.png->getLineAsRGB565(d, c.line, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);

  for (int sx = 0; sx <= c.dataX1 && sx < c.width; sx++) {
    const uint16_t col = c.line[sx];
    if (col == 0) continue;
    uint8_t level;
    if (col == c.lastColor) level = c.lastLevel;
    else { level = colorToLevel(col); c.lastColor = col; c.lastLevel = level; }
    if (!level) { c.otherPx++; continue; }
    c.rainPx++;

    const int16_t px = c.colPx[sx];
    if (inPanel && px >= 0) RadarGrid::setMax(c.grid, px, py, level);

    if (c.detect && level >= c.minLevel) {
      const float kmX = c.colKm[sx];
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

// Decode one radar PNG into grid. open(png) opens the image (from RAM or
// straight from the network) and returns a PNGdec result code.
template <typename Opener>
bool decodeWith(Opener open, uint8_t* grid, bool detect,
                double homeLat, double homeLon, float radiusKm, uint8_t minLevel,
                RainInfo* info) {
  PNG* png = s_png;
  if (!png) return false;
  bool ok = false;
  DecodeCtx c = {};
  c.png = png;

  if (open(png) == PNG_SUCCESS) {
    c.width  = png->getWidth();
    c.height = png->getHeight();
    // PNGdec keeps two image lines in a fixed buffer (PNG_MAX_BUFFERED_PIXELS).
    // The ČHMÚ images are palette PNGs and fit easily; check anyway.
    const int pitch = png->getBufferSize() / (c.height > 0 ? c.height : 1);
    if (2 * (pitch + 1) > (int)PNG_MAX_BUFFERED_PIXELS) {
      DEBUG_LOG("CHMU", "PNG too wide for PNGdec (%d x %d, bpp %d)",
                c.width, c.height, png->getBpp());
      png->close();
      return false;
    }
    c.line   = (uint16_t*)malloc(c.width * sizeof(uint16_t));
    c.colPx  = (int16_t*) malloc(c.width * sizeof(int16_t));
    c.colKm  = (float*)   malloc(c.width * sizeof(float));
    if (c.line && c.colPx && c.colKm) {
      const float kmPerDegLon = 111.32f * cosf(homeLat * 0.017453293f);
      for (int sx = 0; sx < c.width; sx++) {
        const float lon = IMG_LON_LEFT + sx * (IMG_LON_RIGHT - IMG_LON_LEFT) / (c.width - 1);
        float fx, fy;
        RadarMap::projectF(50.0f, lon, &fx, &fy);
        const int16_t px = (int16_t)floorf(fx);
        c.colPx[sx] = (px >= 0 && px < RadarMap::WIDTH) ? px : -1;
        c.colKm[sx] = (lon - (float)homeLon) * kmPerDegLon;
      }
      c.dataX1   = (int)((DATA_LON_RIGHT - IMG_LON_LEFT) * (c.width - 1) / (IMG_LON_RIGHT - IMG_LON_LEFT));
      const float myTop = mercY(IMG_LAT_TOP), myBot = mercY(IMG_LAT_BOTTOM);
      c.dataY0   = (int)ceilf((myTop - mercY(DATA_LAT_TOP)) * (c.height - 1) / (myTop - myBot));
      c.grid     = grid;
      c.detect   = detect;
      c.homeLat  = (float)homeLat;
      c.radiusKm = radiusKm;
      c.minLevel = minLevel;
      c.info     = info;

      memset(grid, 0, RadarGrid::BYTES);
      ok = (png->decode(&c, 0) == PNG_SUCCESS);
      if (!ok) DEBUG_LOG("CHMU", "PNG decode error %d", png->getLastError());
      else DEBUG_LOG("CHMU", "  %d x %d px, rain %lu px, other coloured %lu px", c.width, c.height,
                     (unsigned long)c.rainPx, (unsigned long)c.otherPx);
    } else {
      DEBUG_LOG("CHMU", "Out of memory for line buffers (%d px)", c.width);
    }
    png->close();
  } else {
    DEBUG_LOG("CHMU", "PNG open error %d", png->getLastError());
  }
  free(c.line);
  free(c.colPx);
  free(c.colKm);
  return ok;
}

// Download + decode one frame straight from the HTTPS stream (no file buffer).
PngStream::Source s_src;                 // 4 kB window, static: not on the task stack

struct FrameJob {
  uint8_t*  grid;
  bool      detect;
  double    homeLat, homeLon;
  float     radiusKm;
  uint8_t   minLevel;
  RainInfo* info;
  long      size    = 0;
  bool      started = false;             // body reached the decoder
};

bool decodeBody(Net::ReadFn read, void* stream, long size, void* user) {
  FrameJob& j = *static_cast<FrameJob*>(user);
  j.started = true;
  j.size    = size;
  s_src.reset(read, stream, (int32_t)size);
  PngStream::current = &s_src;
  const bool ok = decodeWith([](PNG* p) {
                               return p->open("", PngStream::openCb, PngStream::closeCb,
                                              PngStream::readCb, PngStream::seekCb, onLine);
                             },
                             j.grid, j.detect, j.homeLat, j.homeLon, j.radiusKm, j.minLevel, j.info);
  PngStream::current = nullptr;
  if (s_src.error) {
    if (s_src.seekBack) DEBUG_LOG("CHMU", "Stream: seek back to %ld, window starts at %ld",
                                  (long)s_src.errorPos, (long)s_src.winStart);
    else                DEBUG_LOG("CHMU", "Stream: data ended at %ld of %ld B",
                                  (long)s_src.errorPos, (long)s_src.size);
  }
  return ok && !s_src.error;
}

}  // namespace

namespace CHMU {

bool begin() {
  if (s_png) return true;
  void* mem = Net::allocLarge(sizeof(PNG));
  if (!mem) {
    DEBUG_LOG("CHMU", "ERROR: no memory for PNG decoder (%u B)", (unsigned)sizeof(PNG));
    return false;
  }
  s_png = new (mem) PNG();
  DEBUG_LOG("CHMU", "PNG decoder ready (%u B)", (unsigned)sizeof(PNG));
  return true;
}

namespace {

// One complete set of frames.
uint8_t fetchFrom(uint8_t* grids, uint8_t maxFrames,
                  double homeLat, double homeLon, float radiusKm, uint8_t minLevel,
                  RainInfo* info) {

  // 1) Find the newest file names in the directory listing
  Index* ix = new (std::nothrow) Index();
  if (!ix) return 0;
  ix->want = maxFrames;
  const long indexBytes = Net::getScan(INDEX_URL, scanIndex, ix, "CHMU");
  if (indexBytes <= 0 || ix->count == 0) {
    DEBUG_LOG("CHMU", "Index failed (%ld B, %u names)", indexBytes, ix->count);
    delete ix;
    return 0;
  }
  DEBUG_LOG("CHMU", "Index %ld B, newest %s", indexBytes, ix->stamp[ix->count - 1]);

  // 2) Download and decode them one by one, oldest first
  RainInfo newest;
  uint8_t done = 0;
  for (uint8_t i = 0; i < ix->count; i++) {
    char url[160];
    snprintf(url, sizeof(url), "%s%s", INDEX_URL, ix->name[i]);
    const bool isNewest = (i == ix->count - 1);
    uint8_t* grid = grids + done * RadarGrid::BYTES;

    // Normally decoded while it downloads – no buffer for the whole file.
    FrameJob job;
    job.grid = grid; job.detect = isNewest; job.homeLat = homeLat; job.homeLon = homeLon;
    job.radiusKm = radiusKm; job.minLevel = minLevel; job.info = &newest;
    bool ok = Net::getStream(url, decodeBody, &job, "CHMU");
    long len = job.size;

    if (!ok && !job.started) {
      // No Content-Length (chunked answer): fall back to downloading into RAM.
      uint8_t* png = nullptr;
      len = Net::getToBuffer(url, &png, MAX_PNG, "CHMU");
      if (len >= 8 && memcmp(png, "\x89PNG", 4) == 0) {
        ok = decodeWith([png, len](PNG* p) { return p->openRAM(png, (int)len, onLine); },
                        grid, isNewest, homeLat, homeLon, radiusKm, minLevel, &newest);
      }
      free(png);
    }
    if (!ok) {
      DEBUG_LOG("CHMU", "Frame %s: download / decode failed", ix->stamp[i]);
      break;
    }
    DEBUG_LOG("CHMU", "Frame %s: %ld B decoded", ix->stamp[i], len);
    done++;
  }

  if (done == ix->count) {
    memcpy(newest.timeUtc, ix->stamp[ix->count - 1], 13);
    *info = newest;
    if (newest.nearby) {
      DEBUG_LOG("CHMU", "Rain within %.0f km: nearest %.0f km, max %u dBZ",
                radiusKm, newest.nearestKm, RadarPalette::levelToDbz(newest.maxLevel));
    } else {
      DEBUG_LOG("CHMU", "No rain >= %u dBZ within %.0f km",
                RadarPalette::levelToDbz(minLevel), radiusKm);
    }
  } else {
    done = 0;                                              // incomplete set -> ignore
  }
  delete ix;
  return done;
}

}  // namespace

uint8_t fetch(uint8_t* grids, uint8_t maxFrames, double homeLat, double homeLon,
              float radiusKm, uint8_t minLevel, RainInfo* info) {
  if (maxFrames > MAX_FRAMES) maxFrames = MAX_FRAMES;
  if (!s_png) return 0;
  return fetchFrom(grids, maxFrames, homeLat, homeLon, radiusKm, minLevel, info);
}

}  // namespace CHMU
