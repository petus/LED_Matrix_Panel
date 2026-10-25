/**
 * @file    screens.cpp
 * @brief   Clock, aircraft and radar screens (see screens.h).
 */
#include "screens.h"
#include "display.h"
#include "colors.h"
#include "map_view.h"
#include "radar_map.h"
#include "qrcode.h"
#include "weather_icons.h"
#include "config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr int16_t CHAR_W = 6;     // built-in 5x7 font incl. spacing

// Czech names without diacritics – the built-in font is plain ASCII.
const char* const WEEKDAYS[] = { "NE", "PO", "UT", "ST", "CT", "PA", "SO" };
const char* const MONTHS[]   = { "LED", "UNO", "BRE", "DUB", "KVE", "CER",
                                 "CVC", "SRP", "ZAR", "RIJ", "LIS", "PRO" };

void textAt(int16_t x, int16_t y, uint16_t color, const char* text) {
  auto& gfx = Display::gfx();
  gfx.setTextSize(1);
  gfx.setTextColor(color);
  gfx.setCursor(x, y);
  gfx.print(text);
}

// Text centred around column cx.
void textCentered(int16_t cx, int16_t y, uint16_t color, const char* text) {
  const int16_t w = strlen(text) * CHAR_W - 1;
  textAt(cx - w / 2, y, color, text);
}

// Like textAt, but '.' takes only 4 px – IP addresses fit in less space.
int16_t textCompactWidth(const char* text) {
  int16_t w = 0;
  for (const char* p = text; *p; p++) w += (*p == '.') ? 4 : CHAR_W;
  return w - 1;
}
void textCompact(int16_t x, int16_t y, uint16_t color, const char* text) {
  char one[2] = { 0, 0 };
  for (const char* p = text; *p; p++) {
    one[0] = *p;
    if (*p == '.') { textAt(x - 1, y, color, one); x += 4; }
    else           { textAt(x, y, color, one);     x += CHAR_W; }
  }
}

// =============================================================================
//  Clock – classic dial with anti-aliased hands
//
//  The dial is rendered into a small RGB canvas first. Every pixel stores the
//  brightest colour drawn onto it, and lines are anti-aliased: pixels at the
//  edge of a hand get a dimmer copy of its colour. On an LED panel this turns
//  1-pixel "staircases" into smooth, slightly thicker hands.
// =============================================================================
constexpr float CLOCK_CX = 47.5f;      // between pixels -> symmetric dial
constexpr float CLOCK_CY = 23.5f;

struct Rgb { uint8_t r, g, b; };

Rgb rgbFrom24(uint32_t c) { return { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }; }

// Labels of the set-up / IP screens (the clock has its own colours in the settings)
const uint16_t COL_DATE_LABEL = rgb565(150, 150, 165);
const uint16_t COL_NO_TIME    = rgb565(230, 230, 240);

namespace Canvas {
  // Only the square around the dial (x 24..71) – saves RAM on boards without PSRAM.
  constexpr int X0 = 24;
  constexpr int W  = 48;
  constexpr int H  = RadarMap::HEIGHT;
  uint8_t buf[H][W][3];

  void clear() { memset(buf, 0, sizeof(buf)); }

  // Brightest-wins plot of colour c scaled by alpha 0..1.
  void plot(int x, int y, Rgb c, float alpha) {
    x -= X0;
    if (x < 0 || y < 0 || x >= W || y >= H || alpha <= 0) return;
    if (alpha > 1) alpha = 1;
    uint8_t* p = buf[y][x];
    const uint8_t v[3] = { (uint8_t)(c.r * alpha), (uint8_t)(c.g * alpha), (uint8_t)(c.b * alpha) };
    for (int k = 0; k < 3; k++) if (v[k] > p[k]) p[k] = v[k];
  }

  // Anti-aliased line of half-width w: the line is sampled every 1/3 px and
  // each nearby pixel gets coverage = w - distance + 0.5 (clamped to 0..1).
  void line(float x0, float y0, float x1, float y1, Rgb c, float w) {
    const float len = hypotf(x1 - x0, y1 - y0);
    const int   n   = (int)ceilf(len * 3) + 1;
    const int   r   = (int)ceilf(w) + 1;
    for (int i = 0; i <= n; i++) {
      const float t = (float)i / n;
      const float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
      const int   cx = (int)lroundf(x), cy = (int)lroundf(y);
      for (int yy = cy - r; yy <= cy + r; yy++) {
        for (int xx = cx - r; xx <= cx + r; xx++) {
          plot(xx, yy, c, w - hypotf(xx - x, yy - y) + 0.5f);
        }
      }
    }
  }

  // Line along a clock angle (0° = 12 o'clock, clockwise) from radius r0 to r1.
  void radial(float angleDeg, float r0, float r1, Rgb c, float w) {
    const float a = angleDeg * 0.017453293f;
    const float sx = sinf(a), sy = -cosf(a);
    line(CLOCK_CX + sx * r0, CLOCK_CY + sy * r0, CLOCK_CX + sx * r1, CLOCK_CY + sy * r1, c, w);
  }

  void blit() {
    auto& gfx = Display::gfx();
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        const uint8_t* p = buf[y][x];
        if (p[0] | p[1] | p[2]) gfx.drawPixel(X0 + x, y, rgb565(p[0], p[1], p[2]));
      }
    }
  }
}  // namespace Canvas

// 12 marks: long ones at 12/3/6/9 in the dial colour, the others shorter
// and at ~45 % brightness.
void drawDial(uint32_t color) {
  const Rgb major = rgbFrom24(color);
  const Rgb minor = { (uint8_t)(major.r * 45 / 100), (uint8_t)(major.g * 45 / 100),
                      (uint8_t)(major.b * 45 / 100) };
  for (int i = 0; i < 12; i++) {
    const bool quarter = (i % 3 == 0);
    Canvas::radial(i * 30.0f, quarter ? 18.5f : 20.5f, 22.5f,
                   quarter ? major : minor, quarter ? 0.75f : 0.45f);
  }
}

// =============================================================================
//  Plane
// =============================================================================
constexpr int16_t PLANE_CX = 18;
constexpr int16_t PLANE_CY = 23;
constexpr int16_t INFO_X   = 38;

const uint16_t COL_PLANE  = rgb565(255, 200, 0);
const uint16_t COL_NAME   = rgb565(255, 255, 255);
const uint16_t COL_TYPE   = rgb565(0, 200, 255);
const uint16_t COL_VALUE  = rgb565(0, 255, 80);
const uint16_t COL_DIST   = rgb565(150, 150, 160);

// Plane silhouette pointing up (north). '#' = lit pixel.
// Rotated at run time by the ground track, so one picture serves all headings.
const char* const PLANE_BITMAP[] = {
  ".......#.......",
  "......###......",
  "......###......",
  "......###......",
  ".....#####.....",
  "....#######....",
  "...####.####...",
  "..###..#..###..",
  ".###...#...###.",
  "##.....#.....##",
  ".......#.......",
  ".......#.......",
  "......###......",
  ".....##.##.....",
  "....##...##....",
};
constexpr int8_t PLANE_W = 15;
constexpr int8_t PLANE_H = 15;
constexpr float  PLANE_SCALE = 1.5f;   // drawn 1.5x larger than the bitmap

bool planeBit(float x, float y) {          // x, y relative to bitmap centre
  const int col = (int)floorf(x + PLANE_W / 2.0f);
  const int row = (int)floorf(y + PLANE_H / 2.0f);
  if (col < 0 || row < 0 || col >= PLANE_W || row >= PLANE_H) return false;
  return PLANE_BITMAP[row][col] == '#';
}

// Draw the bitmap rotated by headingDeg (0 = north, clockwise).
// Inverse mapping: for every screen pixel find the bitmap pixel it shows,
// with 2x2 sub-samples so thin parts do not break up at odd angles.
void drawPlaneIcon(float headingDeg) {
  const float a = headingDeg * 0.017453293f;
  const float c = cosf(a), s = sinf(a);
  constexpr int R = 13;
  for (int dy = -R; dy <= R; dy++) {
    for (int dx = -R; dx <= R; dx++) {
      bool on = false;
      for (int sub = 0; sub < 4 && !on; sub++) {
        const float px = dx + (sub & 1 ? 0.25f : -0.25f);
        const float py = dy + (sub & 2 ? 0.25f : -0.25f);
        // rotate the screen offset back by -heading
        on = planeBit((px * c + py * s) / PLANE_SCALE, (-px * s + py * c) / PLANE_SCALE);
      }
      if (on) Display::gfx().drawPixel(PLANE_CX + dx, PLANE_CY + dy, COL_PLANE);
    }
  }
}

// =============================================================================
//  Radar
// =============================================================================
constexpr int16_t RADAR_TIME_X = 66;
constexpr int16_t RADAR_TIME_Y = 1;
constexpr int16_t RADAR_DOTS_Y = 10;

const uint16_t COL_RADAR_TIME = rgb565(220, 220, 230);
const uint16_t COL_DOT_ON     = rgb565(255, 255, 255);
const uint16_t COL_DOT_OFF    = rgb565(60, 60, 70);

const uint16_t COL_QR_LIGHT   = rgb565(90, 90, 90);      // light QR modules (dim: less current)

}  // namespace

namespace Screens {

void clock(const struct tm* t, uint16_t ms, const Settings::Values& cfg) {
  if (!t) {
    Canvas::clear();
    drawDial(cfg.colorDial);
    Canvas::blit();
    textCentered(48, 16, COL_NO_TIME, "--:--");
    textCentered(48, 26, rgb565From24(cfg.colorDateLabel), "cas...");
    return;
  }
  char buf[40];

  if (!cfg.clockAnalog) {
    // Digital: big HH:MM, seconds bar, date line
    const uint16_t col = rgb565From24(cfg.colorDigits);
    auto& gfx = Display::gfx();
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    gfx.setTextSize(3);                        // 18 px per character
    gfx.setTextColor(col);
    gfx.setCursor(4, cfg.clockDate ? 5 : 12);
    gfx.print(buf);
    gfx.setTextSize(1);
    if (cfg.clockSeconds) {
      const int16_t y = cfg.clockDate ? 30 : 38;
      const int16_t w = (t->tm_sec * 1000 + ms) * 88 / 60000;
      gfx.drawFastHLine(4, y, 88, rgb565(30, 30, 40));
      if (w > 0) gfx.drawFastHLine(4, y, w, rgb565From24(cfg.colorSecond));
    }
    if (cfg.clockDate) {
      snprintf(buf, sizeof(buf), "%s %d.%d.%d", WEEKDAYS[t->tm_wday], t->tm_mday,
               t->tm_mon + 1, t->tm_year + 1900);
      textCentered(48, 36, rgb565From24(cfg.colorDateLabel), buf);
    }
    return;
  }

  // Analog. Hand angles in degrees, 0 = 12 o'clock. Milliseconds make the
  // second hand glide instead of jumping.
  const Rgb hands  = rgbFrom24(cfg.colorHands);
  const Rgb second = rgbFrom24(cfg.colorSecond);
  const float sec  = t->tm_sec + ms / 1000.0f;
  const float min  = t->tm_min + sec / 60.0f;
  const float hour = (t->tm_hour % 12) + min / 60.0f;

  Canvas::clear();
  drawDial(cfg.colorDial);
  Canvas::radial(hour * 30, 0, 11.5f, hands, 1.2f);              // hour
  Canvas::radial(min * 6,   0, 17.5f, hands, 0.8f);              // minute
  if (cfg.clockSeconds) {
    Canvas::radial(sec * 6 + 180, 0, 5, second, 0.45f);          // counterweight
    Canvas::radial(sec * 6,   0, 19.5f, second, 0.45f);          // second
  }
  // Centre cap: 3x3 "plus" with dimmed corners
  const Rgb cap = cfg.clockSeconds ? second : hands;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      Canvas::plot((int)lroundf(CLOCK_CX) + dx, (int)lroundf(CLOCK_CY) + dy, cap,
                   (dx && dy) ? 0.4f : 1.0f);
    }
  }
  Canvas::blit();

  if (cfg.clockDate) {                         // date on both sides of the dial
    const uint16_t label = rgb565From24(cfg.colorDateLabel);
    const uint16_t value = rgb565From24(cfg.colorDateValue);
    textCentered(12, 12, label, WEEKDAYS[t->tm_wday]);
    snprintf(buf, sizeof(buf), "%d.", t->tm_mday);
    textCentered(12, 26, value, buf);
    textCentered(84, 12, label, MONTHS[t->tm_mon]);
    snprintf(buf, sizeof(buf), "%d", t->tm_year + 1900);
    textCentered(84, 26, value, buf);
  }
}

void plane(const Aircraft* a, uint8_t inRange) {
  if (!a) {
    drawPlaneIcon(0);
    textAt(INFO_X, 12, COL_DIST, "zadne");
    textAt(INFO_X, 22, COL_DIST, "letadlo");
    return;
  }
  drawPlaneIcon(a->hasTrack ? a->trackDeg : 0);

  // Name: callsign, else registration, else ICAO address
  const char* name = a->callsign[0] ? a->callsign : (a->reg[0] ? a->reg : a->hex);
  textAt(INFO_X, 1, COL_NAME, name);

  // Aircraft type, or the registration when the type is unknown
  char buf[16];
  const char* second = a->type[0] ? a->type : (name != a->reg ? a->reg : "");
  textAt(INFO_X, 11, COL_TYPE, second);

  snprintf(buf, sizeof(buf), "%d km/h", (int)lroundf(a->speedKmh));
  textAt(INFO_X, 21, COL_VALUE, buf);

  snprintf(buf, sizeof(buf), "%d m", (int)lroundf(a->altM / 10.0f) * 10);
  textAt(INFO_X, 31, COL_VALUE, buf);

  snprintf(buf, sizeof(buf), "%.1f km", a->distKm);
  textAt(INFO_X, 41, COL_DIST, buf);

  // More aircraft in range: "+1", "+2"... under the icon
  if (inRange > 1) {
    snprintf(buf, sizeof(buf), "+%u", (unsigned)(inRange - 1));
    textAt(2, 41, COL_TYPE, buf);
  }
}

void radar(const uint8_t* grid, uint8_t frame, uint8_t frames, const struct tm* t,
           const Settings::Values& cfg) {
  auto& gfx = Display::gfx();

  if (cfg.mapBackground) MapView::drawLand();
  MapView::drawRain(grid);
  MapView::drawBorder();
  MapView::drawCities(cfg.homeLat, cfg.homeLon, cfg.radarCities);
  if (cfg.radarLegend) MapView::drawLegend();

  // Digital clock in the empty corner above Poland, on a black box
  gfx.fillRect(RADAR_TIME_X - 1, 0, 96 - RADAR_TIME_X + 1, RADAR_DOTS_Y + 2, 0);
  char buf[8];
  if (t) snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
  else   snprintf(buf, sizeof(buf), "--:--");
  textAt(RADAR_TIME_X, RADAR_TIME_Y, COL_RADAR_TIME, buf);

  // Animation progress: one dot per radar frame (5 minutes each)
  for (uint8_t i = 0; i < frames; i++) {
    gfx.drawPixel(RADAR_TIME_X + 1 + i * 3, RADAR_DOTS_Y, i == frame ? COL_DOT_ON : COL_DOT_OFF);
  }
}

// =============================================================================
//  Forecast – three columns of 32 px: now | today | tomorrow
// =============================================================================
namespace {

const uint16_t COL_FC_LABEL = rgb565(150, 150, 165);
const uint16_t COL_FC_NOW   = rgb565(255, 255, 255);
const uint16_t COL_FC_MAX   = rgb565(255, 160, 60);
const uint16_t COL_FC_MIN   = rgb565(70, 150, 255);
const uint16_t COL_FC_WIND  = rgb565(140, 140, 155);
const uint16_t COL_FC_SEP   = rgb565(25, 25, 30);

// Temperature "12°" centred on cx. The font has no degree sign that is the
// same in every GFX version, so it is drawn as a tiny 3x3 ring.
void temperature(int16_t cx, int16_t y, uint16_t col, float t) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%ld", lroundf(t));
  const int16_t textW = strlen(buf) * CHAR_W;
  const int16_t x = cx - (textW + 2) / 2;          // digits + 1 px gap + 3 px ring - 1 px font spacing
  textAt(x, y, col, buf);
  auto& gfx = Display::gfx();
  const int16_t dx = x + textW;
  gfx.drawPixel(dx + 1, y,     col);
  gfx.drawPixel(dx,     y + 1, col);
  gfx.drawPixel(dx + 2, y + 1, col);
  gfx.drawPixel(dx + 1, y + 2, col);
}

}  // namespace

void forecast(const Weather::Forecast* f) {
  auto& gfx = Display::gfx();
  if (!f || !f->valid) {
    textCentered(48, 16, COL_FC_LABEL, "predpoved");
    textCentered(48, 26, COL_FC_LABEL, "se nacita...");
    return;
  }
  gfx.drawFastVLine(31, 0, 48, COL_FC_SEP);
  gfx.drawFastVLine(63, 0, 48, COL_FC_SEP);

  static const char* const LABELS[3] = { "TED", "DNES", "ZITRA" };
  for (uint8_t i = 0; i < 3; i++) {
    const int16_t cx = i * 32 + 15;
    textCentered(cx, 1, COL_FC_LABEL, LABELS[i]);
    if (i == 0) {
      WeatherIcon::draw(cx - 7, 11, Weather::kind(f->code), f->isDay);
      temperature(cx, 30, COL_FC_NOW, f->temp);
      char buf[8];
      snprintf(buf, sizeof(buf), "%ldm/s", lroundf(f->windMs));
      textCentered(cx, 40, COL_FC_WIND, buf);
    } else {
      const Weather::Day& d = f->day[i - 1];
      WeatherIcon::draw(cx - 7, 11, Weather::kind(d.code), true);
      temperature(cx, 30, COL_FC_MAX, d.tMax);
      temperature(cx, 40, COL_FC_MIN, d.tMin);
    }
  }
}

void setupAp(const char* ssid, const char* ip) {
  auto& gfx = Display::gfx();

  // QR code that joins the open network: "WIFI:T:nopass;S:<ssid>;;"
  // Version 3 = 29x29 modules, one LED each, with a 1-LED light border.
  char payload[64];
  snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", ssid);
  static uint8_t modules[106];                 // qrcode_getBufferSize(3)
  QRCode qr;
  if (qrcode_initText(&qr, modules, 3, ECC_LOW, payload) == 0) {
    const int16_t x0 = 1, y0 = (48 - qr.size - 2) / 2;
    gfx.fillRect(x0, y0, qr.size + 2, qr.size + 2, COL_QR_LIGHT);
    for (uint8_t y = 0; y < qr.size; y++) {
      for (uint8_t x = 0; x < qr.size; x++) {
        if (qrcode_getModule(&qr, x, y)) gfx.drawPixel(x0 + 1 + x, y0 + 1 + y, 0);
      }
    }
  }

  // Texts on the right (ASCII only – built-in font)
  const int16_t tx = 34;
  textAt(tx, 1, COL_DATE_LABEL, "WiFi:");
  const size_t prefixLen = strlen(AP_SSID_PREFIX);           // "LED-Matrix-"
  char line[16];
  snprintf(line, sizeof(line), "%.*s", (int)(prefixLen - 1), ssid);  // "LED-Matrix"
  textAt(tx, 10, COL_NAME, line);
  textAt(tx, 19, COL_NAME, ssid + prefixLen - 1);                    // "-1A2B"
  textAt(tx, 30, COL_DATE_LABEL, "web:");
  textCompact(96 - textCompactWidth(ip) - 1, 39, COL_TYPE, ip);
}

void connecting(const char* ssid, uint32_t ms) {
  textCentered(48, 8, COL_DATE_LABEL, "Pripojuji WiFi");
  char name[17];
  snprintf(name, sizeof(name), "%s", ssid);
  textCentered(48, 20, COL_NAME, name);
  // Three dots running left to right
  const uint8_t phase = (ms / 300) % 4;
  for (uint8_t i = 0; i < 3; i++) {
    Display::gfx().fillRect(40 + i * 6, 34, 3, 3, i < phase ? COL_TYPE : rgb565(30, 30, 40));
  }
}

void ipAddress(const char* ip, const char* hostname, const char* ssid) {
  textCentered(48, 2, COL_VALUE, "WiFi OK");
  textCompact(48 - textCompactWidth(ip) / 2, 14, COL_NAME, ip);
  char host[24];
  snprintf(host, sizeof(host), "%s.local", hostname);
  textCompact(48 - textCompactWidth(host) / 2, 26, COL_TYPE, host);
  char name[17];
  snprintf(name, sizeof(name), "%s", ssid);
  textCentered(48, 38, COL_DATE_LABEL, name);
}

}  // namespace Screens
