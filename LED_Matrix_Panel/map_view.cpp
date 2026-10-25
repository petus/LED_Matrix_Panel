/**
 * @file    map_view.cpp
 * @brief   Map layers: land, border, cities, home, legend, rain.
 */
#include "map_view.h"
#include "config.h"
#include "display.h"
#include "colors.h"
#include "radar_map.h"
#include "radar_palette.h"

namespace {

// Very dark green – just visible, does not compete with the rain colours.
const uint16_t COLOR_LAND   = rgb565(8, 24, 8);
const uint16_t COLOR_BORDER = rgb565(110, 110, 120);
const uint16_t COLOR_CITY   = rgb565(0, 80, 255);     // blue, like the arms at home
const uint16_t COLOR_HOME   = rgb565(0, 255, 0);
const uint16_t COLOR_HOME_C = COLOR_CITY;              // arms when home = city

constexpr int16_t LEGEND_X    = 91;
constexpr int16_t LEGEND_W    = 4;
constexpr int16_t LEGEND_Y    = 17;
constexpr int16_t LEGEND_STEP = 2;

struct City {
  const char* name;
  float lat;
  float lon;
};

// Regional capitals spread over the whole country, so the map stays readable.
const City CITIES[] = {
  { "Praha",            50.0755f, 14.4378f },
  { "Plzen",            49.7384f, 13.3736f },
  { "Liberec",          50.7671f, 15.0562f },
  { "Jihlava",          49.3961f, 15.5912f },
  { "Brno",             49.1951f, 16.6068f },
  { "Ostrava",          49.8209f, 18.2625f },
  { "Ceske Budejovice", 48.9745f, 14.4743f },
  { "Hradec Kralove",   50.2092f, 15.8328f },
  { "Usti nad Labem",   50.6607f, 14.0323f },
  { "Olomouc",          49.5938f, 17.2509f },
};
constexpr uint8_t CITY_COUNT = sizeof(CITIES) / sizeof(CITIES[0]);

void drawCross(int16_t x, int16_t y, uint16_t centre, uint16_t arms) {
  auto& gfx = Display::gfx();
  gfx.drawPixel(x - 1, y, arms);
  gfx.drawPixel(x + 1, y, arms);
  gfx.drawPixel(x, y - 1, arms);
  gfx.drawPixel(x, y + 1, arms);
  gfx.drawPixel(x, y, centre);
}

}  // namespace

namespace MapView {

void drawLand() {
  auto& gfx = Display::gfx();
  for (int16_t y = 0; y < RadarMap::HEIGHT; y++) {
    for (int16_t x = 0; x < RadarMap::WIDTH; x++) {
      if (RadarMap::at(x, y) == ':') gfx.drawPixel(x, y, COLOR_LAND);
    }
  }
}

void drawBorder() {
  auto& gfx = Display::gfx();
  for (int16_t y = 0; y < RadarMap::HEIGHT; y++) {
    for (int16_t x = 0; x < RadarMap::WIDTH; x++) {
      if (RadarMap::at(x, y) == '#') gfx.drawPixel(x, y, COLOR_BORDER);
    }
  }
}

void drawCities(double homeLat, double homeLon, bool showCities) {
  int16_t hx, hy;
  RadarMap::project(homeLat, homeLon, &hx, &hy);
  bool homeOnCity = false;

  // Home "on" a city: the projected points may differ by one pixel (a few
  // km) – e.g. a geocoded address in Prague vs. the city centre – so a
  // neighbouring pixel counts too. The cross is then drawn on the city
  // position with a green centre and blue arms.
  for (uint8_t i = 0; showCities && i < CITY_COUNT; i++) {
    int16_t x, y;
    RadarMap::project(CITIES[i].lat, CITIES[i].lon, &x, &y);
    if (!homeOnCity && abs(x - hx) <= 1 && abs(y - hy) <= 1) {
      drawCross(x, y, COLOR_HOME, COLOR_HOME_C);
      homeOnCity = true;
    } else {
      drawCross(x, y, COLOR_CITY, COLOR_CITY);
    }
  }
  if (!homeOnCity) Display::gfx().drawPixel(hx, hy, COLOR_HOME);
}

void drawLegend() {
  auto& gfx = Display::gfx();
  for (uint8_t level = 1; level <= RadarPalette::LEVELS; level++) {
    const int16_t y = LEGEND_Y + (RadarPalette::LEVELS - level) * LEGEND_STEP;
    gfx.fillRect(LEGEND_X, y, LEGEND_W, LEGEND_STEP, RadarPalette::color(level));
  }
}

void drawRain(const uint8_t* grid) {
  if (!grid) return;
  auto& gfx = Display::gfx();
  for (int16_t y = 0; y < RadarMap::HEIGHT; y++) {
    for (int16_t x = 0; x < RAIN_MAX_X; x++) {
      const uint8_t level = RadarGrid::get(grid, x, y);
      if (level) gfx.drawPixel(x, y, RadarPalette::color(level));
    }
  }
}

}  // namespace MapView
