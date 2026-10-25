/**
 * @file    config.h
 * @brief   Central project configuration – the only file you usually need to edit.
 *
 * Hardware:
 *   - LaskaKit ESP32-S3-DEVKit  (https://github.com/LaskaKit/ESP32-S3-DEVKit)
 *   - Waveshare RGB LED matrix 96x48 (flexible), pitch 2.5 mm, HUB75, 1/24 scan
 *
 * Hardware (pins, panel type) is set here at compile time.
 * Behaviour (position, radii, thresholds, colours, brightness ...) is set in
 * the web interface and stored in flash – the DEFAULT_* values below are only
 * used on the very first start or after a factory reset.
 */
#pragma once

#include <Arduino.h>

// =============================================================================
//  Firmware identification
// =============================================================================
#define FW_NAME     "LED Matrix Panel"
#define FW_VERSION  "1.0.0"

// =============================================================================
//  Debug output (Serial Monitor)
// =============================================================================
// 1 = debug messages are printed to the Serial Monitor
// 0 = all debug code is removed at compile time (zero overhead)
#define DEBUG_ENABLED     1

// Serial speed – set the same value in the Serial Monitor.
#define DEBUG_BAUDRATE    115200

// 1 = single-key commands over Serial (m = screen mode, i = info, ...)
//     Works only when DEBUG_ENABLED is 1.
#define DEBUG_CONSOLE     1

// =============================================================================
//  Panel geometry
// =============================================================================
#define PANEL_WIDTH       96      // pixels per panel (horizontal)
#define PANEL_HEIGHT      48      // pixels per panel (vertical)
// Number of panels chained IN -> OUT (this firmware is laid out for one).
#define PANEL_CHAIN       1

// Brightness before the settings are loaded (0–255). A full white 96x48
// panel can draw up to ~4 A at 5 V.
#define PANEL_BRIGHTNESS  60

// How a frame gets to the panel:
//   Screens draw into an ordinary RGB565 canvas in RAM (9 kB). Display::show()
//   compares it with what the panel shows and writes only the pixels that
//   changed into ONE DMA buffer – so nothing is ever cleared or swapped on
//   the panel itself: no flicker, no double buffer (saves ~43 kB of RAM).
//   Colour depth is the library default (8 bit) – no build_opt.h needed.

// Column driver IC on the panel. Waveshare uses SHIFTREG for 96x48-F.
//   HUB75_I2S_CFG::SHIFTREG  – plain shift register (default, most panels)
//   HUB75_I2S_CFG::FM6124, FM6126A, ICN2038S, MBI5124, DP3246 – panels that
//   need an init sequence (symptom: full lit rows or random stripes)
#define PANEL_DRIVER      HUB75_I2S_CFG::SHIFTREG

// Row (line) driver on the panel.
//   HUB75_I2S_CFG::SM5368   – Waveshare 96x48-F **V2** (sticker "V2" on the back).
//                             Rows are selected by a serial shift register
//                             driven through A/B/C, lines D and E are unused.
//                             Requires library v3.1.0 or newer.
//   HUB75_I2S_CFG::TYPE138  – classic address lines A–E (V1 panels, most others)
#define PANEL_LINE_DECODER  HUB75_I2S_CFG::SM5368

// HUB75 clock and minimum refresh rate – the values of the Waveshare example,
// the same as in the first test sketch (HUB75_Panel) that showed no flicker.
#define PANEL_I2S_SPEED        HUB75_I2S_CFG::HZ_16M
#define PANEL_MIN_REFRESH_HZ   240

// Dark clock pulses around each row change (1–8, 0 = library default 4).
// Random flickering pixels at the panel edge: try other values live with
// the serial console key "l", then put the best one here.
#define PANEL_LATCH_BLANKING   0

// Clock edge on which the panel latches data.
// Waveshare panels need false – with true the image is shifted by one pixel
// and text becomes unreadable. Other (generic) panels usually need true.
#define PANEL_CLK_PHASE   false

// =============================================================================
//  HUB75 wiring  (ESP32-S3 GPIO numbers)
// =============================================================================
//  HUB75 IN connector (looking at the panel back, key notch up)
//  with the ESP32-S3 GPIO used for each signal:
//
//      GPIO  Signal   Pin  Pin   Signal  GPIO
//        4   R1      [ 1][ 2]    G1        5
//        6   B1      [ 3][ 4]    GND     GND
//        7   R2      [ 5][ 6]    G2       15
//       16   B2      [ 7][ 8]    E        21
//       17   A       [ 9][10]    B        18
//        8   C       [11][12]    D        41
//       38   CLK     [13][14]    LAT      40
//       39   OE      [15][16]    GND     GND
//
//  Pins chosen on the LaskaKit DEVKit avoid: strapping pins (0, 3, 45, 46),
//  onboard RGB LED (45), USB (19, 20), UART0 (43, 44), uSup I2C (2, 42),
//  uSup power enable (47), battery ADC (9), octal PSRAM (35, 36, 37).
//
//  Left header  : 4 5 6 7 15 16 17 18 8   -> colour data + address A/B/C
//  Right header : 41 40 39 38 21          -> address D, E + control signals

// Colour data – upper half of the panel (rows 0–15)
#define PIN_R1   4
#define PIN_G1   5
#define PIN_B1   6
// Colour data – lower half of the panel (rows 16–31)
#define PIN_R2   7
#define PIN_G2   15
#define PIN_B2   16

// 1 = swap red and blue in software (for panels wired R<->B inside).
// The start-up test shows three bars R, G, B – they must light red, green,
// blue. The Waveshare 96x48-F V2 with the wiring above needs 1.
#define PANEL_SWAP_RB  1

// Row address. 96x48 panel = 1/24 scan -> 24 row addresses -> 5 lines A–E.
// (64x32 panels are 1/16 scan and need only A–D.)
#define PIN_A    17
#define PIN_B    18
#define PIN_C    8
#define PIN_D    41
// Address line E – required for 1/24 and 1/32 scan panels (96x48, 64x64).
// Set to -1 for 1/16 scan panels (64x32).
#define PIN_E    21

// Control signals
#define PIN_LAT  40
#define PIN_OE   39
#define PIN_CLK  38

// =============================================================================
//  Access point for the first set-up (open network, no password)
// =============================================================================
#define AP_SSID_PREFIX         "LED-Matrix-"     // + last 4 hex digits of the MAC
#define AP_IP                  "192.168.4.1"
// Without a working home WiFi the access point comes up after this time.
#define WIFI_CONNECT_TIMEOUT_MS  20000
// With a stored home WiFi that was not reachable (router still booting after
// a power cut...), the access point is closed now and then for one more
// attempt – only when no phone is connected to it.
#define WIFI_AP_RETRY_MS         300000
// After the home WiFi connects, the IP address is shown this long.
#define IP_SCREEN_MS             10000

// =============================================================================
//  Default settings – change them in the web interface, not here
// =============================================================================
#define DEFAULT_HOSTNAME        "ledmatrix"
#define DEFAULT_HOME_LAT        50.0755          // Praha
#define DEFAULT_HOME_LON        14.4378
#define DEFAULT_HOME_NAME       "Praha"
#define DEFAULT_TIME_ZONE       "CET-1CEST,M3.5.0,M10.5.0/3"

#define DEFAULT_PLANE_ENABLED   true
#define DEFAULT_PLANE_RADIUS_KM 10               // aircraft closer than this
#define DEFAULT_PLANE_PERIOD_S  10               // adsb.fi allows 1 request/s
#define DEFAULT_PLANE_MIN_SHOW_S 5               // keep the screen at least this long
#define DEFAULT_PLANE_MIN_ALT_M 0                // ignore aircraft lower than this
#define DEFAULT_PLANE_CALLSIGN_ONLY false

#define DEFAULT_RADAR_ENABLED   true
#define DEFAULT_RAIN_RADIUS_KM  100              // rain closer than this
#define DEFAULT_RAIN_MIN_DBZ    20               // weakest echo counted as rain
#define DEFAULT_RADAR_FRAMES    6                // animation length (x 5 min)
#define DEFAULT_ANIM_STEP_MS    500
#define DEFAULT_ANIM_HOLD_MS    2000             // newest frame stays longer
#define DEFAULT_RADAR_LEGEND    true
#define DEFAULT_RADAR_CITIES    true

#define DEFAULT_CLOCK_ANALOG    true             // false = digital
#define DEFAULT_CLOCK_SECONDS   true
#define DEFAULT_CLOCK_DATE      true
#define DEFAULT_COLOR_HANDS     0xE6E6F0         // RGB
#define DEFAULT_COLOR_SECOND    0xFF1E1E
#define DEFAULT_COLOR_DIGITS    0x00C8FF         // digital clock
#define DEFAULT_COLOR_DIAL      0xFFFFFF         // dial marks
#define DEFAULT_COLOR_DATE_LABEL 0x9696A5        // weekday, month
#define DEFAULT_COLOR_DATE_VALUE 0xFFFFFF        // day, year
// Viewing sector for aircraft (e.g. the view from a window)
#define DEFAULT_VIEW_ENABLED    false
#define DEFAULT_VIEW_DIR_DEG    135              // 0 N, 45 NE, 90 E, 135 SE, 180 S ...
#define DEFAULT_VIEW_HALF_DEG   30               // sector = direction ± this
#define DEFAULT_VIEW_RANGE_KM   5
// Screens
#define DEFAULT_SCREEN_MODE      0               // 0 auto, see Station::Mode
#define DEFAULT_ROTATE_MASK      0x0F            // bit 0 clock, 1 forecast, 2 radar, 3 aircraft
#define DEFAULT_ROTATE_S         15              // rotation: seconds per screen
#define DEFAULT_ROTATE_RADAR_DRY false           // rotation: radar also without rain nearby
#define DEFAULT_AUTO_PLANE       0               // auto: 0 at once, 1 now and then, 2 never
#define DEFAULT_AUTO_PLANE_EVERY_S 60            // "now and then": pause between ...
#define DEFAULT_AUTO_PLANE_SHOW_S  15            // ... and showing time
#define DEFAULT_MAP_BACKGROUND   true            // radar: dark green land
// Forecast screen (Open-Meteo): shown between clock periods
#define DEFAULT_FORECAST_ENABLED true
#define DEFAULT_FORECAST_EVERY_S 50              // clock this long ...
#define DEFAULT_FORECAST_SHOW_S  10              // ... then the forecast this long

#define DEFAULT_BRIGHT_DAY      60               // 0–255
#define DEFAULT_BRIGHT_NIGHT    10
#define DEFAULT_NIGHT_MODE      0                // 0 off, 1 time range, 2 sunset–sunrise
#define DEFAULT_NIGHT_START_MIN (22 * 60)        // minutes after midnight
#define DEFAULT_NIGHT_END_MIN   (6 * 60)
#define DEFAULT_NIGHT_ACTION    0                // 0 dim, 1 display off
#define DEFAULT_NIGHT_CLOCK_ONLY true            // no aircraft / radar at night

// =============================================================================
//  Fixed limits and timing
// =============================================================================
#define RADAR_FRAMES_MAX       6                 // buffers are sized for this
// Forecast download period and how old it may get before it is not shown
#define FORECAST_PERIOD_MS    (15UL * 60UL * 1000UL)
#define FORECAST_RETRY_MS     (60UL * 1000UL)
#define FORECAST_MAX_AGE_MS   (3UL * 60UL * 60UL * 1000UL)
#define RADAR_PERIOD_MS        (5UL * 60UL * 1000UL)   // ČHMÚ: new image every 5 min
#define RADAR_RETRY_MS         (60UL * 1000UL)         // after a failed download
#define FRAME_INTERVAL_MS      20                // 50 FPS

// Sent with every HTTP request – free APIs ask for an identifiable client.
#define HTTP_USER_AGENT        "LED-Matrix-Panel/" FW_VERSION " (+https://chiptron.cz)"
