/**
 * @file    settings.cpp
 * @brief   Settings storage (Preferences / NVS) and JSON conversion.
 */
#include "settings.h"
#include "config.h"
#include "debug.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

namespace {

constexpr const char* NVS_NAMESPACE = "ledpanel";
constexpr const char* KEY_VALUES    = "cfg";
constexpr const char* KEY_LAYOUT    = "cfgver";
constexpr const char* KEY_SSID      = "ssid";
constexpr const char* KEY_PASS      = "pass";

// Bump when Values changes so old blobs are not misread.
constexpr uint16_t LAYOUT_VERSION = 1;

Settings::Values  s_values;
SemaphoreHandle_t s_mutex = nullptr;

void copyStr(char* dst, size_t cap, const char* src) {
  snprintf(dst, cap, "%s", src ? src : "");
}

template <typename T>
T clampT(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

// "#RRGGBB" <-> 0xRRGGBB
void colorToText(uint32_t c, char* out) { snprintf(out, 8, "#%06lX", (unsigned long)(c & 0xFFFFFF)); }
bool textToColor(const char* t, uint32_t* c) {
  if (!t || t[0] != '#' || strlen(t) != 7) return false;
  char* end;
  const unsigned long v = strtoul(t + 1, &end, 16);
  if (*end) return false;
  *c = v;
  return true;
}

// "HH:MM" <-> minutes after midnight
void minutesToText(uint16_t m, char* out) {
  m %= 1440;                                   // 0..23:59
  snprintf(out, 6, "%02u:%02u", (unsigned)(m / 60) % 24, (unsigned)m % 60);
}
bool textToMinutes(const char* t, uint16_t* m) {
  unsigned h, mi;
  if (!t || sscanf(t, "%u:%u", &h, &mi) != 2 || h > 23 || mi > 59) return false;
  *m = h * 60 + mi;
  return true;
}

void save(const Settings::Values& v) {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putBytes(KEY_VALUES, &v, sizeof(v));
  p.putUShort(KEY_LAYOUT, LAYOUT_VERSION);
  p.end();
}

}  // namespace

namespace Settings {

namespace {

// Factory defaults (do not touch WiFi).
Values defaults() {
  Values v;
  memset(&v, 0, sizeof(v));
  copyStr(v.hostname, sizeof(v.hostname), DEFAULT_HOSTNAME);
  copyStr(v.timeZone, sizeof(v.timeZone), DEFAULT_TIME_ZONE);
  v.homeLat = DEFAULT_HOME_LAT;
  v.homeLon = DEFAULT_HOME_LON;
  copyStr(v.homeName, sizeof(v.homeName), DEFAULT_HOME_NAME);

  v.planeEnabled      = DEFAULT_PLANE_ENABLED;
  v.planeRadiusKm     = DEFAULT_PLANE_RADIUS_KM;
  v.planePeriodS      = DEFAULT_PLANE_PERIOD_S;
  v.planeMinShowS     = DEFAULT_PLANE_MIN_SHOW_S;
  v.planeMinAltM      = DEFAULT_PLANE_MIN_ALT_M;
  v.planeCallsignOnly = DEFAULT_PLANE_CALLSIGN_ONLY;

  v.radarEnabled = DEFAULT_RADAR_ENABLED;
  v.rainRadiusKm = DEFAULT_RAIN_RADIUS_KM;
  v.rainMinDbz   = DEFAULT_RAIN_MIN_DBZ;
  v.radarFrames  = DEFAULT_RADAR_FRAMES;
  v.animStepMs   = DEFAULT_ANIM_STEP_MS;
  v.animHoldMs   = DEFAULT_ANIM_HOLD_MS;
  v.radarLegend  = DEFAULT_RADAR_LEGEND;
  v.radarCities  = DEFAULT_RADAR_CITIES;

  v.clockAnalog  = DEFAULT_CLOCK_ANALOG;
  v.clockSeconds = DEFAULT_CLOCK_SECONDS;
  v.clockDate    = DEFAULT_CLOCK_DATE;
  v.colorHands   = DEFAULT_COLOR_HANDS;
  v.colorSecond  = DEFAULT_COLOR_SECOND;
  v.colorDigits  = DEFAULT_COLOR_DIGITS;
  v.colorDial      = DEFAULT_COLOR_DIAL;
  v.colorDateLabel = DEFAULT_COLOR_DATE_LABEL;
  v.colorDateValue = DEFAULT_COLOR_DATE_VALUE;
  v.forecastEnabled = DEFAULT_FORECAST_ENABLED;
  v.forecastEveryS  = DEFAULT_FORECAST_EVERY_S;
  v.forecastShowS   = DEFAULT_FORECAST_SHOW_S;
  v.viewEnabled = DEFAULT_VIEW_ENABLED;
  v.viewDirDeg  = DEFAULT_VIEW_DIR_DEG;
  v.viewHalfDeg = DEFAULT_VIEW_HALF_DEG;
  v.viewRangeKm = DEFAULT_VIEW_RANGE_KM;
  v.screenMode      = DEFAULT_SCREEN_MODE;
  v.rotateMask      = DEFAULT_ROTATE_MASK;
  v.rotateS         = DEFAULT_ROTATE_S;
  v.rotateRadarDry  = DEFAULT_ROTATE_RADAR_DRY;
  v.autoPlane       = DEFAULT_AUTO_PLANE;
  v.autoPlaneEveryS = DEFAULT_AUTO_PLANE_EVERY_S;
  v.autoPlaneShowS  = DEFAULT_AUTO_PLANE_SHOW_S;
  v.mapBackground   = DEFAULT_MAP_BACKGROUND;
  v.radarSource     = DEFAULT_RADAR_SOURCE;
  v.language        = DEFAULT_LANGUAGE;

  v.brightDay      = DEFAULT_BRIGHT_DAY;
  v.brightNight    = DEFAULT_BRIGHT_NIGHT;
  v.nightMode      = DEFAULT_NIGHT_MODE;
  v.nightStartMin  = DEFAULT_NIGHT_START_MIN;
  v.nightEndMin    = DEFAULT_NIGHT_END_MIN;
  v.nightAction    = DEFAULT_NIGHT_ACTION;
  v.nightClockOnly = DEFAULT_NIGHT_CLOCK_ONLY;
  return v;
}

}  // namespace

void begin() {
  s_mutex = xSemaphoreCreateMutex();
  s_values = defaults();

  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  // Fields are only ever added at the end of Values. A shorter blob comes
  // from an older firmware: take what it has, the new fields keep defaults.
  // (It is copied only up to the first new field – the old blob may end
  // with padding bytes that would land in it.)
  const size_t len = p.getBytesLength(KEY_VALUES);
  bool stored = false;
  if (p.getUShort(KEY_LAYOUT, 0) == LAYOUT_VERSION && len > 0 && len <= sizeof(Values)) {
    static Values tmp;
    if (p.getBytes(KEY_VALUES, &tmp, sizeof(tmp)) == len) {
      // Where each older layout ended (the first field it did not have yet).
      static const size_t ENDS[] = { offsetof(Values, colorDial), offsetof(Values, forecastEnabled),
                                     offsetof(Values, viewEnabled), offsetof(Values, screenMode),
                                     offsetof(Values, radarSource) };
      size_t take = len;
      if (len != sizeof(Values)) {
        take = 0;
        for (size_t e : ENDS) if (e <= len && e > take) take = e;
      }
      memcpy(&s_values, &tmp, take);
      stored = true;
      if (len != sizeof(Values)) DEBUG_LOG("CFG", "Older settings layout (%u B), new fields set to defaults", (unsigned)len);
    }
  }
  p.end();

  // Values out of range (e.g. bytes that were padding in an older layout)
  if (s_values.radarSource > RADAR_SRC_MAX) s_values.radarSource = DEFAULT_RADAR_SOURCE;
  if (s_values.language > LANG_EN)          s_values.language    = DEFAULT_LANGUAGE;

  // Make sure strings are terminated even if flash held garbage.
  s_values.hostname[sizeof(s_values.hostname) - 1] = '\0';
  s_values.timeZone[sizeof(s_values.timeZone) - 1] = '\0';
  s_values.homeName[sizeof(s_values.homeName) - 1] = '\0';

  DEBUG_LOG("CFG", "%s, position %.4f %.4f (%s)", stored ? "Loaded" : "Defaults",
            s_values.homeLat, s_values.homeLon, s_values.homeName);
}

Values get() {
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  const Values v = s_values;
  xSemaphoreGive(s_mutex);
  return v;
}

void set(const Values& v) {
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  s_values = v;
  xSemaphoreGive(s_mutex);
  save(v);
  DEBUG_LOG("CFG", "Saved");
}

void factoryReset() {
  set(defaults());
  clearWifi();
  DEBUG_LOG("CFG", "Factory reset");
}

// -----------------------------------------------------------------------------
//  WiFi
// -----------------------------------------------------------------------------
void wifi(char* ssid, size_t ssidCap, char* pass, size_t passCap) {
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  copyStr(ssid, ssidCap, p.getString(KEY_SSID, "").c_str());
  copyStr(pass, passCap, p.getString(KEY_PASS, "").c_str());
  p.end();
}

void setWifi(const char* ssid, const char* pass) {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString(KEY_SSID, ssid);
  p.putString(KEY_PASS, pass);
  p.end();
  DEBUG_LOG("CFG", "WiFi \"%s\" saved", ssid);
}

void clearWifi() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.remove(KEY_SSID);
  p.remove(KEY_PASS);
  p.end();
}

// -----------------------------------------------------------------------------
//  JSON
// -----------------------------------------------------------------------------
void toJson(const Values& v, JsonObject o) {
  char buf[8];
  o["hostname"] = v.hostname;
  o["tz"]       = v.timeZone;
  o["lat"]      = v.homeLat;
  o["lon"]      = v.homeLon;
  o["place"]    = v.homeName;

  o["planeOn"]       = v.planeEnabled;
  o["planeRadius"]   = v.planeRadiusKm;
  o["planePeriod"]   = v.planePeriodS;
  o["planeMinShow"]  = v.planeMinShowS;
  o["planeMinAlt"]   = v.planeMinAltM;
  o["planeCallsign"] = v.planeCallsignOnly;

  o["radarOn"]     = v.radarEnabled;
  o["rainRadius"]  = v.rainRadiusKm;
  o["rainDbz"]     = v.rainMinDbz;
  o["radarFrames"] = v.radarFrames;
  o["animStep"]    = v.animStepMs;
  o["animHold"]    = v.animHoldMs;
  o["legend"]      = v.radarLegend;
  o["cities"]      = v.radarCities;

  o["clockAnalog"]  = v.clockAnalog;
  o["clockSeconds"] = v.clockSeconds;
  o["clockDate"]    = v.clockDate;
  colorToText(v.colorHands, buf);  o["colHands"]  = String(buf);
  colorToText(v.colorSecond, buf); o["colSecond"] = String(buf);
  colorToText(v.colorDigits, buf); o["colDigits"] = String(buf);
  colorToText(v.colorDial, buf);      o["colDial"]      = String(buf);
  colorToText(v.colorDateLabel, buf); o["colDateLabel"] = String(buf);
  colorToText(v.colorDateValue, buf); o["colDateValue"] = String(buf);
  o["fcOn"]    = v.forecastEnabled;
  o["fcEvery"] = v.forecastEveryS;
  o["fcShow"]  = v.forecastShowS;
  o["viewOn"]    = v.viewEnabled;
  o["viewDir"]   = v.viewDirDeg;
  o["viewHalf"]  = v.viewHalfDeg;
  o["viewRange"] = v.viewRangeKm;
  o["rotClock"]    = (v.rotateMask & 1) != 0;
  o["rotForecast"] = (v.rotateMask & 2) != 0;
  o["rotRadar"]    = (v.rotateMask & 4) != 0;
  o["rotPlane"]    = (v.rotateMask & 8) != 0;
  o["rotS"]        = v.rotateS;
  o["rotRadarDry"] = v.rotateRadarDry;
  o["autoPlane"]   = v.autoPlane;
  o["autoPlaneEvery"] = v.autoPlaneEveryS;
  o["autoPlaneShow"]  = v.autoPlaneShowS;
  o["mapBg"]       = v.mapBackground;
  o["radarSrc"]    = v.radarSource;
  o["lang"]        = v.language == LANG_EN ? "en" : "cs";

  o["brightDay"]   = v.brightDay;
  o["brightNight"] = v.brightNight;
  o["nightMode"]   = v.nightMode;
  minutesToText(v.nightStartMin, buf); o["nightStart"] = String(buf);
  minutesToText(v.nightEndMin, buf);   o["nightEnd"]   = String(buf);
  o["nightAction"]    = v.nightAction;
  o["nightClockOnly"] = v.nightClockOnly;
}

void fromJson(JsonObjectConst j, Values& v) {
  if (j["hostname"].is<const char*>()) {
    // Host names: letters, digits and '-' only
    char h[sizeof(v.hostname)];
    size_t n = 0;
    for (const char* p = j["hostname"].as<const char*>(); *p && n < sizeof(h) - 1; p++) {
      const char c = tolower(*p);
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') h[n++] = c;
    }
    h[n] = '\0';
    if (n) copyStr(v.hostname, sizeof(v.hostname), h);
  }
  if (j["tz"].is<const char*>() && strlen(j["tz"].as<const char*>()) > 2) {
    copyStr(v.timeZone, sizeof(v.timeZone), j["tz"]);
  }
  if (j["lat"].is<double>()) v.homeLat = clampT(j["lat"].as<double>(), -85.0, 85.0);
  if (j["lon"].is<double>()) v.homeLon = clampT(j["lon"].as<double>(), -180.0, 180.0);
  if (j["place"].is<const char*>()) copyStr(v.homeName, sizeof(v.homeName), j["place"]);

  if (j["planeOn"].is<bool>())       v.planeEnabled      = j["planeOn"];
  if (j["planeRadius"].is<int>())    v.planeRadiusKm     = clampT(j["planeRadius"].as<int>(), 1, 100);
  if (j["planePeriod"].is<int>())    v.planePeriodS      = clampT(j["planePeriod"].as<int>(), 5, 300);
  if (j["planeMinShow"].is<int>())   v.planeMinShowS     = clampT(j["planeMinShow"].as<int>(), 0, 120);
  if (j["planeMinAlt"].is<int>())    v.planeMinAltM      = clampT(j["planeMinAlt"].as<int>(), 0, 15000);
  if (j["planeCallsign"].is<bool>()) v.planeCallsignOnly = j["planeCallsign"];

  if (j["radarOn"].is<bool>())     v.radarEnabled = j["radarOn"];
  if (j["rainRadius"].is<int>())   v.rainRadiusKm = clampT(j["rainRadius"].as<int>(), 5, 500);
  if (j["rainDbz"].is<int>())      v.rainMinDbz   = clampT(j["rainDbz"].as<int>(), 4, 60);
  if (j["radarFrames"].is<int>())  v.radarFrames  = clampT(j["radarFrames"].as<int>(), 1, RADAR_FRAMES_MAX);
  if (j["animStep"].is<int>())     v.animStepMs   = clampT(j["animStep"].as<int>(), 100, 5000);
  if (j["animHold"].is<int>())     v.animHoldMs   = clampT(j["animHold"].as<int>(), 0, 10000);
  if (j["legend"].is<bool>())      v.radarLegend  = j["legend"];
  if (j["cities"].is<bool>())      v.radarCities  = j["cities"];

  if (j["clockAnalog"].is<bool>())  v.clockAnalog  = j["clockAnalog"];
  if (j["clockSeconds"].is<bool>()) v.clockSeconds = j["clockSeconds"];
  if (j["clockDate"].is<bool>())    v.clockDate    = j["clockDate"];
  textToColor(j["colHands"]  | "", &v.colorHands);
  textToColor(j["colSecond"] | "", &v.colorSecond);
  textToColor(j["colDigits"] | "", &v.colorDigits);
  textToColor(j["colDial"]      | "", &v.colorDial);
  textToColor(j["colDateLabel"] | "", &v.colorDateLabel);
  textToColor(j["colDateValue"] | "", &v.colorDateValue);

  if (j["fcOn"].is<bool>())    v.forecastEnabled = j["fcOn"];
  if (j["fcEvery"].is<int>())  v.forecastEveryS  = clampT(j["fcEvery"].as<int>(), 5, 3600);
  if (j["fcShow"].is<int>())   v.forecastShowS   = clampT(j["fcShow"].as<int>(), 3, 600);

  if (j["viewOn"].is<bool>())   v.viewEnabled = j["viewOn"];
  if (j["viewDir"].is<int>())   v.viewDirDeg  = ((j["viewDir"].as<int>() % 360) + 360) % 360;
  if (j["viewHalf"].is<int>())  v.viewHalfDeg = clampT(j["viewHalf"].as<int>(), 5, 90);
  if (j["viewRange"].is<int>()) v.viewRangeKm = clampT(j["viewRange"].as<int>(), 1, 50);

  // Rotation screens: each key alone sets / clears its bit
  static const char* const ROT_KEYS[4] = { "rotClock", "rotForecast", "rotRadar", "rotPlane" };
  for (uint8_t i = 0; i < 4; i++) {
    if (j[ROT_KEYS[i]].is<bool>()) {
      if (j[ROT_KEYS[i]].as<bool>()) v.rotateMask |= (1 << i);
      else                           v.rotateMask &= ~(1 << i);
    }
  }
  if (j["rotS"].is<int>())            v.rotateS         = clampT(j["rotS"].as<int>(), 3, 3600);
  if (j["rotRadarDry"].is<bool>())    v.rotateRadarDry  = j["rotRadarDry"];
  if (j["autoPlane"].is<int>())       v.autoPlane       = clampT(j["autoPlane"].as<int>(), 0, 2);
  if (j["autoPlaneEvery"].is<int>())  v.autoPlaneEveryS = clampT(j["autoPlaneEvery"].as<int>(), 5, 3600);
  if (j["autoPlaneShow"].is<int>())   v.autoPlaneShowS  = clampT(j["autoPlaneShow"].as<int>(), 3, 600);
  if (j["mapBg"].is<bool>())          v.mapBackground   = j["mapBg"];
  if (j["radarSrc"].is<int>())        v.radarSource     = clampT(j["radarSrc"].as<int>(), 0, RADAR_SRC_MAX);
  if (j["lang"].is<const char*>())    v.language        = strcmp(j["lang"].as<const char*>(), "en") == 0 ? LANG_EN : LANG_CS;

  if (j["brightDay"].is<int>())   v.brightDay   = clampT(j["brightDay"].as<int>(), 1, 255);
  if (j["brightNight"].is<int>()) v.brightNight = clampT(j["brightNight"].as<int>(), 1, 255);
  if (j["nightMode"].is<int>())   v.nightMode   = clampT(j["nightMode"].as<int>(), 0, 2);
  textToMinutes(j["nightStart"] | "", &v.nightStartMin);
  textToMinutes(j["nightEnd"] | "", &v.nightEndMin);
  if (j["nightAction"].is<int>())     v.nightAction    = clampT(j["nightAction"].as<int>(), 0, 1);
  if (j["nightClockOnly"].is<bool>()) v.nightClockOnly = j["nightClockOnly"];
}

}  // namespace Settings
