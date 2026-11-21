/**
 * @file    settings.h
 * @brief   User settings – edited in the web interface, stored in flash (NVS).
 *
 * Values are kept in one struct. Readers take a copy (get()), the web
 * interface writes a whole new struct (set()), which is saved at once.
 * Both are protected by a mutex because the network task (core 0) and the
 * display loop (core 1) read the settings at the same time.
 *
 * WiFi credentials are stored under separate keys, so a firmware update that
 * changes the settings layout falls back to defaults but keeps the WiFi.
 */
#pragma once

#include <stdint.h>
#include <ArduinoJson.h>

namespace Settings {

  struct Values {
    // Device
    char     hostname[32];
    char     timeZone[48];        // POSIX TZ string
    // Position
    double   homeLat;
    double   homeLon;
    char     homeName[48];        // only for display in the web interface
    // Aircraft
    bool     planeEnabled;
    uint16_t planeRadiusKm;
    uint16_t planePeriodS;
    uint16_t planeMinShowS;
    uint16_t planeMinAltM;
    bool     planeCallsignOnly;
    // Radar
    bool     radarEnabled;
    uint16_t rainRadiusKm;
    uint8_t  rainMinDbz;
    uint8_t  radarFrames;         // 1..RADAR_FRAMES_MAX
    uint16_t animStepMs;
    uint16_t animHoldMs;
    bool     radarLegend;
    bool     radarCities;
    // Clock
    bool     clockAnalog;         // false = digital
    bool     clockSeconds;
    bool     clockDate;
    uint32_t colorHands;          // 0xRRGGBB
    uint32_t colorSecond;
    uint32_t colorDigits;
    // Display
    uint8_t  brightDay;           // 0..255
    uint8_t  brightNight;
    uint8_t  nightMode;           // 0 off, 1 time range, 2 sunset..sunrise
    uint16_t nightStartMin;       // minutes after midnight
    uint16_t nightEndMin;
    uint8_t  nightAction;         // 0 dim, 1 display off
    bool     nightClockOnly;      // no aircraft / radar screens at night
    // Clock colours (added later – new fields always go to the END, so
    // settings saved by an older firmware still load, see Settings::begin)
    uint32_t colorDial;           // hour marks of the dial (minute marks dimmer)
    uint32_t colorDateLabel;      // weekday, month, digital date line
    uint32_t colorDateValue;      // day, year
    // Forecast (added later)
    bool     forecastEnabled;
    uint16_t forecastEveryS;      // clock this long between two forecasts
    uint16_t forecastShowS;       // forecast shown this long
    // Aircraft viewing sector (added later)
    bool     viewEnabled;
    uint16_t viewDirDeg;          // 0 = north, clockwise
    uint8_t  viewHalfDeg;         // sector = viewDir ± viewHalf
    uint16_t viewRangeKm;         // own range, independent of planeRadiusKm
    // Screens (added later)
    uint8_t  screenMode;          // Station::Mode, remembered over restarts
    uint8_t  rotateMask;          // bit 0 clock, 1 forecast, 2 radar, 3 aircraft
    uint16_t rotateS;             // rotation: seconds per screen
    bool     rotateRadarDry;      // rotation: radar also when it does not rain nearby
    uint8_t  autoPlane;           // auto mode: 0 at once, 1 now and then, 2 never
    uint16_t autoPlaneEveryS;     // "now and then": pause between two showings
    uint16_t autoPlaneShowS;      // "now and then": aircraft shown this long
    bool     mapBackground;       // radar: draw the land
    // Radar source and language (added later). Both 0 = the old behaviour,
    // so padding bytes of an older blob (always zero) load as the defaults.
    uint8_t  radarSource;         // RADAR_SRC_* (config.h)
    uint8_t  language;            // LANG_CS / LANG_EN
  };

  /// Load from flash (defaults when empty or from an older layout).
  void begin();

  /// Copy of the current values.
  Values get();

  /// Replace all values and save them to flash.
  void set(const Values& v);

  /// Back to defaults and forget the WiFi.
  void factoryReset();

  // WiFi credentials
  void wifi(char* ssid, size_t ssidCap, char* pass, size_t passCap);
  void setWifi(const char* ssid, const char* pass);
  void clearWifi();

  // JSON form used by the web interface and for export / import.
  void toJson(const Values& v, JsonObject out);
  /// Apply the keys present in json onto v; values are range-checked.
  void fromJson(JsonObjectConst json, Values& v);

}  // namespace Settings
