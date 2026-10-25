/**
 * @file    map_view.h
 * @brief   Drawing of the Czech map layers for the radar screen.
 *
 * Typical order of one radar frame:
 *   MapView::drawLand();          dim land, outside stays black
 *   ... rain pixels ...
 *   MapView::drawBorder();        border on top of the rain
 *   MapView::drawCities();        red crosses + green home marker
 *   MapView::drawLegend();        dBZ colour scale on the right
 */
#pragma once

#include <stdint.h>

namespace MapView {

  /// Rain is drawn only left of this column – the right edge holds the legend.
  constexpr int16_t RAIN_MAX_X = 86;

  void drawLand();
  void drawBorder();

  /// Cities as red 5-LED crosses (when showCities), home position as a
  /// green LED. When home falls on a city: green centre, blue arms.
  void drawCities(double homeLat, double homeLon, bool showCities);

  /// Vertical colour scale, 60 dBZ at the top.
  void drawLegend();

  /// Draw a radar grid (see radar_palette.h) – only pixels with rain.
  void drawRain(const uint8_t* grid);

}  // namespace MapView
