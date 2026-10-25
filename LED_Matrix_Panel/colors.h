/**
 * @file    colors.h
 * @brief   Colour helpers and a basic palette in RGB565 format.
 *
 * The HUB75 library (like Adafruit GFX) uses 16-bit RGB565 colours:
 *   RRRRR GGGGGG BBBBB
 * rgb565() converts the usual 8-bit R, G, B values at compile time,
 * rgb565From24() converts a 0xRRGGBB value (as used in the web settings).
 */
#pragma once

#include <stdint.h>

// Convert 8-bit R, G, B (0–255) to a 16-bit RGB565 colour.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// 0xRRGGBB -> RGB565
constexpr uint16_t rgb565From24(uint32_t rgb) {
  return rgb565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}
