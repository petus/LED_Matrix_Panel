/**
 * @file    display.cpp
 * @brief   HUB75 panel initialisation, canvas and transfer (see display.h).
 */
#include "display.h"
#include <driver/gpio.h>
#include "config.h"
#include "debug.h"
#include "colors.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace {
  // Created in begin(), live for the whole program.
  MatrixPanel_I2S_DMA* s_panel      = nullptr;
  uint8_t              s_drive      = PANEL_DRIVE_STRENGTH;
  GFXcanvas16*         s_canvas     = nullptr;   // what the screens draw
  uint16_t*            s_shown      = nullptr;   // what the panel shows now
  uint8_t              s_brightness = PANEL_BRIGHTNESS;
  uint8_t              s_latch      = 0;

  constexpr int16_t W = PANEL_WIDTH * PANEL_CHAIN;
  constexpr int16_t H = PANEL_HEIGHT;
}

namespace Display {

bool begin() {
  DEBUG_LOG("DISP", "Init %dx%d, chain %d, single DMA buffer + canvas",
            PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN);

  // Order is fixed by the library: R1 G1 B1 R2 G2 B2 A B C D E LAT OE CLK
#if PANEL_SWAP_RB
  // Panel with red and blue swapped: drive the R lines with the blue data.
  HUB75_I2S_CFG::i2s_pins pins = {
    PIN_B1, PIN_G1, PIN_R1,
    PIN_B2, PIN_G2, PIN_R2,
#else
  HUB75_I2S_CFG::i2s_pins pins = {
    PIN_R1, PIN_G1, PIN_B1,
    PIN_R2, PIN_G2, PIN_B2,
#endif
    PIN_A,  PIN_B,  PIN_C, PIN_D, PIN_E,
    PIN_LAT, PIN_OE, PIN_CLK
  };

  DEBUG_LOG("DISP", "Pins  R1=%d G1=%d B1=%d  R2=%d G2=%d B2=%d  (R/B swap %s)",
            PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PANEL_SWAP_RB ? "ON" : "off");
  DEBUG_LOG("DISP", "Pins  A=%d B=%d C=%d D=%d E=%d  LAT=%d OE=%d CLK=%d",
            PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_LAT, PIN_OE, PIN_CLK);

#if PANEL_DISABLED
  // Diagnostic build: the panel is never driven (no DMA, no clock on the
  // HUB75 lines). Drawing still goes into the canvas, so nothing else changes.
  // Static levels only (no clock): all lines low, OE high = LEDs off.
  const int8_t lines[] = { PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2,
                           PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_LAT, PIN_CLK };
  for (int8_t p : lines) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(PIN_OE, OUTPUT);
  digitalWrite(PIN_OE, HIGH);
  (void)pins;
  DEBUG_LOG("DISP", "PANEL_DISABLED: panel NOT driven (WiFi test build), lines static, OE high");
  s_canvas = new GFXcanvas16(W, H);
  s_shown  = (uint16_t*)calloc((size_t)W * H, sizeof(uint16_t));
  if (!s_canvas || !s_canvas->getBuffer() || !s_shown) return false;
  s_canvas->setTextWrap(false);
  return true;
#endif

  HUB75_I2S_CFG config(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN, pins);
  config.double_buff      = false;              // see config.h: canvas + diff instead
  config.clkphase         = PANEL_CLK_PHASE;
  config.driver           = PANEL_DRIVER;
  config.line_decoder     = PANEL_LINE_DECODER;
  config.i2sspeed         = PANEL_I2S_SPEED;
  config.min_refresh_rate = PANEL_MIN_REFRESH_HZ;
  if (PANEL_LATCH_BLANKING) config.latch_blanking = PANEL_LATCH_BLANKING;
  s_latch = config.latch_blanking;

  s_panel = new MatrixPanel_I2S_DMA(config);

#if DEBUG_ENABLED
  const uint32_t heapBefore = ESP.getFreeHeap();
#endif
  if (!s_panel->begin()) {
    DEBUG_LOG("DISP", "ERROR: begin() failed – not enough memory for DMA?");
    return false;
  }
  DEBUG_LOG("DISP", "DMA buffer %lu B, %u bit colour, refresh %d Hz, latch blanking %u",
            (unsigned long)(heapBefore - ESP.getFreeHeap()),
            (unsigned)config.getPixelColorDepthBits(), s_panel->calculated_refresh_rate,
            (unsigned)s_latch);

  s_canvas = new GFXcanvas16(W, H);
  s_shown  = (uint16_t*)calloc((size_t)W * H, sizeof(uint16_t));
  if (!s_canvas || !s_canvas->getBuffer() || !s_shown) {
    DEBUG_LOG("DISP", "ERROR: no memory for the canvas (2 x %u B)", (unsigned)(W * H * 2));
    return false;
  }
  s_canvas->setTextWrap(false);

  s_panel->clearScreen();                       // matches s_shown (all black)
  setDriveStrength(PANEL_DRIVE_STRENGTH);       // after begin(): the library sets up the pins
  setBrightness(s_brightness);
  DEBUG_LOG("DISP", "Ready");
  return true;
}

Adafruit_GFX& gfx() {
  return *s_canvas;
}

void clear() {
  s_canvas->fillScreen(0);
}

void show() {
  const uint16_t* src = s_canvas->getBuffer();
  uint32_t i = 0;
  for (int16_t y = 0; y < H; y++) {
    for (int16_t x = 0; x < W; x++, i++) {
      const uint16_t c = src[i];
      if (c != s_shown[i]) {
        s_shown[i] = c;
        if (s_panel) s_panel->drawPixel(x, y, c);
      }
    }
  }
}

void colourTest() {
  clear();
  auto& g = gfx();
  const uint16_t cols[3] = { rgb565(255, 0, 0), rgb565(0, 255, 0), rgb565(0, 0, 255) };
  const char*    names[3] = { "R", "G", "B" };
  for (uint8_t i = 0; i < 3; i++) {
    g.fillRect(i * 32 + 4, 4, 24, 28, cols[i]);
    g.setTextColor(rgb565(255, 255, 255));
    g.setTextSize(1);
    g.setCursor(i * 32 + 14, 37);
    g.print(names[i]);
  }
  show();
}

void setBrightness(uint8_t value) {
  s_brightness = value;
  if (s_panel) s_panel->setBrightness8(value);
  DEBUG_LOG("DISP", "Brightness %u", value);
}

uint8_t brightness() {
  return s_brightness;
}

uint8_t setDriveStrength(uint8_t level) {
  if (level > 3) level = 3;
  s_drive = level;
  if (!s_panel) return s_drive;
  const int8_t pins[] = { PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2,
                          PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_LAT, PIN_OE, PIN_CLK };
  for (int8_t p : pins) gpio_set_drive_capability((gpio_num_t)p, (gpio_drive_cap_t)level);
  static const char* const MA[] = { "~5", "~10", "~20", "~40" };
  DEBUG_LOG("DISP", "Pin drive strength %u (%s mA), panel clock %d MHz", level, MA[level], PANEL_I2S_MHZ);
  return s_drive;
}

uint8_t driveStrength() {
  return s_drive;
}

uint8_t setLatchBlanking(uint8_t pulses) {
  if (!s_panel) return s_latch;
  s_latch = s_panel->setLatBlanking(pulses);
  s_panel->setBrightness8(s_brightness);        // the blanking is applied with the brightness
  DEBUG_LOG("DISP", "Latch blanking %u", (unsigned)s_latch);
  return s_latch;
}

uint8_t latchBlanking() {
  return s_latch;
}

int16_t width()  { return W; }
int16_t height() { return H; }

}  // namespace Display
