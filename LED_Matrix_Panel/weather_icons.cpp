/**
 * @file    weather_icons.cpp
 * @brief   Weather icons (see weather_icons.h).
 *
 * Built from circles and lines so they need no bitmaps. Every icon fits
 * a 16x16 box; the "precipitation" icons put a cloud in the upper part
 * and drops / flakes / a lightning bolt below it.
 */
#include "weather_icons.h"
#include "display.h"
#include "colors.h"

namespace {

const uint16_t COL_SUN        = rgb565(255, 190, 0);
const uint16_t COL_MOON       = rgb565(210, 210, 150);
const uint16_t COL_CLOUD      = rgb565(170, 170, 185);
const uint16_t COL_CLOUD_DARK = rgb565(110, 110, 125);
const uint16_t COL_RAIN       = rgb565(0, 110, 255);
const uint16_t COL_SNOW       = rgb565(230, 230, 255);
const uint16_t COL_BOLT       = rgb565(255, 230, 0);
const uint16_t COL_FOG        = rgb565(130, 130, 140);

void sun(int16_t cx, int16_t cy, int16_t r) {
  auto& g = Display::gfx();
  g.fillCircle(cx, cy, r, COL_SUN);
  // 8 short rays
  static const int8_t DIR[8][2] = { {0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1} };
  for (uint8_t i = 0; i < 8; i++) {
    const int16_t d = (DIR[i][0] && DIR[i][1]) ? r + 1 : r + 2;   // diagonals a bit shorter
    g.drawLine(cx + DIR[i][0] * d, cy + DIR[i][1] * d,
               cx + DIR[i][0] * (d + 1), cy + DIR[i][1] * (d + 1), COL_SUN);
  }
}

void moon(int16_t cx, int16_t cy, int16_t r) {
  auto& g = Display::gfx();
  g.fillCircle(cx, cy, r, COL_MOON);
  g.fillCircle(cx + r / 2 + 1, cy - r / 2, r - 1, 0);          // bite -> crescent
}

// Cloud 14x7 with its top-left corner at (x, y).
void cloud(int16_t x, int16_t y, uint16_t col) {
  auto& g = Display::gfx();
  g.fillCircle(x + 4,  y + 4, 3, col);
  g.fillCircle(x + 8,  y + 3, 3, col);
  g.fillCircle(x + 11, y + 4, 2, col);
  g.fillRect(x + 1, y + 4, 13, 3, col);
}

}  // namespace

namespace WeatherIcon {

void draw(int16_t x, int16_t y, Weather::Kind kind, bool day) {
  auto& g = Display::gfx();
  using K = Weather::Kind;
  switch (kind) {
    case K::Clear:
      if (day) sun(x + 7, y + 7, 3);
      else     moon(x + 7, y + 7, 5);
      break;

    case K::PartlyCloudy:
      if (day) sun(x + 5, y + 5, 2);
      else     moon(x + 5, y + 5, 4);
      cloud(x + 1, y + 7, COL_CLOUD);
      break;

    case K::Cloudy:
      cloud(x + 3, y + 2, COL_CLOUD_DARK);
      cloud(x + 0, y + 6, COL_CLOUD);
      break;

    case K::Fog:
      for (int16_t i = 0; i < 4; i++) {
        const int16_t yy = y + 3 + i * 3;
        g.drawFastHLine(x + 1 + (i & 1) * 2, yy, 12, COL_FOG);
      }
      break;

    case K::Drizzle:
      cloud(x + 1, y + 1, COL_CLOUD);
      for (int16_t i = 0; i < 3; i++) g.drawPixel(x + 4 + i * 4, y + 11 + (i & 1) * 2, COL_RAIN);
      break;

    case K::Rain:
      cloud(x + 1, y + 1, COL_CLOUD_DARK);
      for (int16_t i = 0; i < 3; i++) {
        const int16_t dx = x + 4 + i * 4, dy = y + 10;
        g.drawLine(dx + 1, dy, dx - 1, dy + 4, COL_RAIN);       // slanted drop
      }
      break;

    case K::Snow:
      cloud(x + 1, y + 1, COL_CLOUD);
      for (int16_t i = 0; i < 3; i++) {                         // small "+" flakes
        const int16_t fx = x + 3 + i * 5, fy = y + 12 + (i & 1);
        g.drawPixel(fx, fy, COL_SNOW);
        g.drawPixel(fx - 1, fy, COL_SNOW); g.drawPixel(fx + 1, fy, COL_SNOW);
        g.drawPixel(fx, fy - 1, COL_SNOW); g.drawPixel(fx, fy + 1, COL_SNOW);
      }
      break;

    case K::Storm:
      cloud(x + 1, y + 1, COL_CLOUD_DARK);
      g.drawLine(x + 9, y + 8,  x + 6, y + 12, COL_BOLT);         // zig-zag bolt
      g.drawLine(x + 6, y + 12, x + 9, y + 12, COL_BOLT);
      g.drawLine(x + 9, y + 12, x + 6, y + 15, COL_BOLT);
      break;
  }
}

}  // namespace WeatherIcon
