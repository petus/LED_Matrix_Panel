/**
 * @file    display.h
 * @brief   The HUB75 panel: drawing canvas + transfer to the DMA buffer.
 *
 * The rest of the program talks to the panel only through this module.
 *
 * Drawing is done with the familiar Adafruit GFX API on the canvas returned
 * by Display::gfx() – an RGB565 picture in RAM, e.g.:
 *   Display::clear();
 *   Display::gfx().drawPixel(10, 5, rgb565(255, 0, 0));
 *   Display::gfx().print("Hello");
 *   Display::show();          // only now the panel changes
 *
 * show() writes just the pixels that differ from the picture on the panel,
 * straight into the single DMA buffer. The panel is never cleared or
 * switched between buffers, so it cannot flicker while a frame is drawn.
 */
#pragma once

#include <Adafruit_GFX.h>

namespace Display {

  /// Initialise the panel. Returns false if the buffers cannot be allocated.
  bool begin();

  /// Drawing surface (Adafruit GFX).
  Adafruit_GFX& gfx();

  /// Clear the canvas to black (the panel keeps its picture until show()).
  void clear();

  /// Transfer the canvas to the panel (changed pixels only).
  void show();

  /// Short colour test: red, green and blue bars with their letters.
  void colourTest();

  /// Latch blanking 1–8 (dark clock pulses around the row change), applied
  /// at once. Returns the value set. For tests from the serial console.
  /// Drive strength of the HUB75 pins 0..3 (see config.h). Returns the value set.
  uint8_t setDriveStrength(uint8_t level);
  uint8_t driveStrength();

  uint8_t setLatchBlanking(uint8_t pulses);
  uint8_t latchBlanking();

  /// Brightness 0–255.
  void    setBrightness(uint8_t value);
  uint8_t brightness();

  /// Size in pixels.
  int16_t width();
  int16_t height();

}  // namespace Display
