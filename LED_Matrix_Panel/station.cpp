/**
 * @file    station.cpp
 * @brief   Screen selection, night mode and animation timing (see station.h).
 */
#include "station.h"
#include "config.h"
#include "debug.h"
#include "display.h"
#include "screens.h"
#include "sky_data.h"
#include "fetcher.h"
#include "time_sync.h"
#include "radar_palette.h"
#include "settings.h"
#include "wifi_setup.h"
#include "sun.h"

#include <Arduino.h>

namespace {

enum class Screen : uint8_t { Setup, Connecting, Ip, Off, Clock, Plane, Radar, Forecast };

const char* screenName(Screen s) {
  switch (s) {
    case Screen::Setup:      return "setup AP";
    case Screen::Connecting: return "connecting";
    case Screen::Ip:         return "IP address";
    case Screen::Off:        return "off (night)";
    case Screen::Clock:      return "clock";
    case Screen::Plane:      return "aircraft";
    case Screen::Radar:      return "radar";
    case Screen::Forecast:   return "forecast";
  }
  return "?";
}

Station::Mode s_mode        = Station::Mode::Auto;
Screen        s_screen      = Screen::Connecting;
uint32_t      s_screenSince = 0;
uint32_t      s_idleSince   = 0;         // clock/forecast cycle started here
uint32_t      s_lastFrame   = 0;
uint32_t      s_lastKey     = 0;         // content of the frame on the panel
uint32_t      s_lastDraw    = 0;
uint8_t       s_brightness  = 0;
bool          s_night       = false;

// The aircraft shown last – kept for planeMinShowS after it leaves.
Aircraft      s_shownPlane;
bool          s_havePlane   = false;
uint8_t       s_shownCount  = 0;         // aircraft in range when last seen
uint32_t      s_planeSeenMs = 0;         // last time an aircraft was around

// Radar frame to show, from the time since the radar screen appeared:
// frames step every animStepMs, the newest one stays animHoldMs.
uint8_t radarFrameAt(uint32_t elapsed, uint8_t frames, const Settings::Values& cfg) {
  if (frames <= 1) return 0;
  const uint32_t cycle = (frames - 1) * cfg.animStepMs + cfg.animHoldMs;
  const uint32_t pos   = cycle ? elapsed % cycle : 0;
  const uint8_t  idx   = pos / cfg.animStepMs;
  return idx >= frames ? frames - 1 : idx;
}

// Is it night now (local time t)?
bool isNight(const struct tm& t, const Settings::Values& cfg) {
  const int now = t.tm_hour * 60 + t.tm_min;
  int from, to;
  if (cfg.nightMode == 1) {
    from = cfg.nightStartMin;
    to   = cfg.nightEndMin;
  } else if (cfg.nightMode == 2) {
    // Local offset from UTC right now (handles summer time), then sun times
    const time_t now_t = time(nullptr);
    struct tm utc;
    gmtime_r(&now_t, &utc);
    int offset = (t.tm_hour * 60 + t.tm_min) - (utc.tm_hour * 60 + utc.tm_min);
    if (t.tm_yday != utc.tm_yday) offset += (t.tm_yday > utc.tm_yday || t.tm_yday == 0) ? 1440 : -1440;
    if (offset > 900)  offset -= 1440;           // year boundary
    if (offset < -900) offset += 1440;
    int rise, set;
    if (!Sun::times(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    cfg.homeLat, cfg.homeLon, offset, &rise, &set)) return false;
    from = set;
    to   = rise;
  } else {
    return false;
  }
  return (from <= to) ? (now >= from && now < to)     // e.g. 01:00–05:00
                      : (now >= from || now < to);    // over midnight
}

// Rotation state
uint8_t  s_rotIdx   = 0;                 // index into ROTATION
uint32_t s_rotSince = 0;                 // current rotation screen shown since
// "Now and then" aircraft in auto mode: cycle starts when an aircraft appears
uint32_t s_planeCycleStart = 0;
bool     s_planeAround     = false;

bool planeAvailable(const SkyData::State& s, const Settings::Values& cfg, uint32_t now) {
  const bool fresh = s.planeValid && (now - s.planeUpdatedMs) < 3UL * cfg.planePeriodS * 1000UL;
  return cfg.planeEnabled && fresh && s.planeNearby;
}

bool forecastAvailable(const SkyData::State& s, const Settings::Values& cfg, uint32_t now) {
  return cfg.forecastEnabled && s.forecast.valid && now - s.forecastUpdatedMs < FORECAST_MAX_AGE_MS;
}

bool rainAvailable(const SkyData::State& s, const Settings::Values& cfg) {
  return cfg.radarEnabled && s.radarFrames > 0 && s.rain.nearby;
}

// Clock, and every forecastEveryS the forecast for forecastShowS. The cycle
// restarts whenever we come back from another screen.
Screen clockOrForecast(const SkyData::State& s, const Settings::Values& cfg, uint32_t now) {
  if (s_screen != Screen::Clock && s_screen != Screen::Forecast) s_idleSince = now;
  if (forecastAvailable(s, cfg, now)) {
    const uint32_t every  = cfg.forecastEveryS * 1000UL;
    const uint32_t period = every + cfg.forecastShowS * 1000UL;
    if ((now - s_idleSince) % period >= every) return Screen::Forecast;
  }
  return Screen::Clock;
}

// --- Auto -------------------------------------------------------------------
Screen autoScreen(const SkyData::State& s, const Settings::Values& cfg, uint32_t now) {
  if (s_night && cfg.nightClockOnly) return Screen::Clock;

  // Aircraft
  const bool plane = planeAvailable(s, cfg, now);
  if (plane && !s_planeAround) s_planeCycleStart = now;    // a new one appeared
  s_planeAround = plane;
  if (plane && cfg.autoPlane == 0) return Screen::Plane;   // at once
  if (plane && cfg.autoPlane == 1) {                       // now and then
    const uint32_t show   = cfg.autoPlaneShowS * 1000UL;
    const uint32_t period = show + cfg.autoPlaneEveryS * 1000UL;
    if ((now - s_planeCycleStart) % period < show) return Screen::Plane;
  }
  // Keep the aircraft a moment after it left, so the screen does not flicker.
  if (cfg.autoPlane == 0 && s_screen == Screen::Plane &&
      now - s_screenSince < cfg.planeMinShowS * 1000UL) return Screen::Plane;

  // Rain nearby -> radar instead of clock / forecast
  if (rainAvailable(s, cfg)) return Screen::Radar;

  return clockOrForecast(s, cfg, now);
}

// --- Rotate -----------------------------------------------------------------
constexpr Screen ROTATION[4] = { Screen::Clock, Screen::Forecast, Screen::Radar, Screen::Plane };

bool rotationHas(uint8_t i, const SkyData::State& s, const Settings::Values& cfg, uint32_t now) {
  if (!(cfg.rotateMask & (1 << i))) return false;            // not chosen
  switch (ROTATION[i]) {
    case Screen::Clock:    return true;
    case Screen::Forecast: return forecastAvailable(s, cfg, now);
    case Screen::Radar:    return cfg.radarEnabled && s.radarFrames > 0 &&
                                  (s.rain.nearby || cfg.rotateRadarDry);
    case Screen::Plane:    return planeAvailable(s, cfg, now);
    default:               return false;
  }
}

Screen rotateScreen(const SkyData::State& s, const Settings::Values& cfg, uint32_t now) {
  if (s_night && cfg.nightClockOnly) return Screen::Clock;
  const bool due = now - s_rotSince >= cfg.rotateS * 1000UL;
  if (due || !rotationHas(s_rotIdx, s, cfg, now)) {
    // Next screen that has something to show (current one last)
    for (uint8_t k = 1; k <= 4; k++) {
      const uint8_t i = (s_rotIdx + k) % 4;
      if (rotationHas(i, s, cfg, now)) {
        if (i != s_rotIdx || due) s_rotSince = now;
        s_rotIdx = i;
        return ROTATION[i];
      }
    }
    return Screen::Clock;                                    // nothing chosen / available
  }
  return ROTATION[s_rotIdx];
}

Screen chooseScreen(const SkyData::State& s, const Settings::Values& cfg,
                    bool timeOk, uint32_t now) {
  // Network set-up first
  const WifiSetup::State ws = WifiSetup::state();
  if (ws == WifiSetup::State::AccessPoint) return Screen::Setup;
  if (ws == WifiSetup::State::Connecting && !timeOk) return Screen::Connecting;
  const uint32_t online = WifiSetup::connectedForMs();
  if (online && online < IP_SCREEN_MS) return Screen::Ip;

  // "Display off at night" applies in every mode
  if (s_night && cfg.nightAction == 1) return Screen::Off;

  switch (s_mode) {
    case Station::Mode::Clock:    return Screen::Clock;
    case Station::Mode::Plane:    return Screen::Plane;
    case Station::Mode::Radar:    return Screen::Radar;
    case Station::Mode::Forecast: return Screen::Forecast;
    case Station::Mode::Rotate:   return rotateScreen(s, cfg, now);
    case Station::Mode::Auto:     break;
  }
  return autoScreen(s, cfg, now);
}

}  // namespace

namespace Station {

void begin() {
  if (psramFound()) {
    DEBUG_LOG("STAT", "PSRAM %lu B", (unsigned long)ESP.getPsramSize());
  } else {
    DEBUG_LOG("STAT", "No PSRAM – everything runs from internal RAM (%lu B free)",
              (unsigned long)ESP.getFreeHeap());
  }
  const Settings::Values cfg = Settings::get();
  s_mode = (cfg.screenMode < Station::MODE_COUNT) ? (Station::Mode)cfg.screenMode : Station::Mode::Auto;
  DEBUG_LOG("STAT", "Screen mode %s (saved)", modeName(s_mode));
  TimeSync::begin(cfg.timeZone);
  if (!SkyData::begin()) return;
  Fetcher::begin();
  s_screenSince = millis();
}

// -----------------------------------------------------------------------------
//  Redraw only when something visible changes
// -----------------------------------------------------------------------------
// Every screen is described by a small number ("key") made from what it shows:
// the screen itself, the second of the clock, the radar frame, the aircraft
// data... When the key is the same as last time, the panel already shows the
// right picture – the DMA keeps refreshing it on its own, without the CPU.
// A redraw is still forced once per second so changed settings (colours,
// options) appear without tracking every one of them.
namespace {

constexpr uint32_t FORCE_REDRAW_MS = 1000;

uint32_t mix(uint32_t h, uint32_t v) {          // FNV-1a style hash step
  for (uint8_t i = 0; i < 4; i++) { h ^= (v >> (i * 8)) & 0xFF; h *= 16777619u; }
  return h;
}

uint32_t mixBytes(uint32_t h, const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}

// Time part of the key: how often the clock picture really changes.
uint32_t clockTick(const struct tm& t, uint16_t ms, const Settings::Values& cfg) {
  const uint32_t sec = t.tm_hour * 3600UL + t.tm_min * 60UL + t.tm_sec;
  if (cfg.clockSeconds) return sec * 20 + ms / 50;   // smooth second hand / bar: 20 fps
  return sec;                                        // otherwise once per second
}

}  // namespace

void update() {
  const uint32_t now = millis();
  if (now - s_lastFrame < FRAME_INTERVAL_MS) return;
  s_lastFrame = now;

  const Settings::Values cfg = Settings::get();
  struct tm local;
  uint16_t   ms = 0;
  const bool timeOk = TimeSync::now(&local, &ms);

  // Night mode and brightness (only sent to the panel when it changes)
  const bool night = timeOk && isNight(local, cfg);
  if (night != s_night) {
    s_night = night;
    DEBUG_LOG("STAT", "Night mode %s", night ? "ON" : "OFF");
  }
  const uint8_t bright = s_night ? cfg.brightNight : cfg.brightDay;
  if (bright != s_brightness) {
    s_brightness = bright;
    Display::setBrightness(bright);
  }

  SkyData::State& s = SkyData::lock();
  const Screen next = chooseScreen(s, cfg, timeOk, now);
  if (next != s_screen) {
    DEBUG_LOG("STAT", "Screen %s -> %s", screenName(s_screen), screenName(next));
    s_screen      = next;
    s_screenSince = now;
  }
  if (planeAvailable(s, cfg, now)) {
    s_shownPlane  = s.plane;
    s_shownCount  = s.planeCount;
    s_havePlane   = true;
    s_planeSeenMs = now;
  } else if (s_havePlane && now - s_planeSeenMs > cfg.planeMinShowS * 1000UL) {
    s_havePlane = false;                         // gone – don't show an old one
  }

  // What would this frame show?
  uint32_t key = mix(2166136261u, (uint32_t)s_screen);
  uint8_t  radarFrame = 0;
  switch (s_screen) {
    case Screen::Setup:
    case Screen::Ip:
    case Screen::Off:
      break;                                     // static pictures
    case Screen::Connecting:
      key = mix(key, (now - s_screenSince) / 300);   // running dots
      break;
    case Screen::Clock:
      key = mix(key, timeOk ? clockTick(local, ms, cfg) : 0xFFFFFFFF);
      break;
    case Screen::Plane:
      key = mix(key, s_havePlane);
      if (s_havePlane) key = mixBytes(key, &s_shownPlane, sizeof(s_shownPlane));
      key = mix(key, s_shownCount);
      break;
    case Screen::Forecast:
      key = mix(key, s.forecastVersion);         // new download
      break;
    case Screen::Radar:
      radarFrame = radarFrameAt(now - s_screenSince, s.radarFrames, cfg);
      key = mix(key, radarFrame);
      key = mix(key, s.radarFrames);
      key = mix(key, s.radarVersion);            // new download
      key = mix(key, timeOk ? local.tm_hour * 60 + local.tm_min : 0xFFFF);
      break;
  }

  if (key == s_lastKey && now - s_lastDraw < FORCE_REDRAW_MS) {
    SkyData::unlock();
    return;                                      // the panel already shows this
  }
  s_lastKey  = key;
  s_lastDraw = now;

  Display::clear();

  switch (s_screen) {
    case Screen::Setup:
      Screens::setupAp(WifiSetup::apSsid(), AP_IP);
      break;
    case Screen::Connecting:
      Screens::connecting(WifiSetup::staSsid(), now - s_screenSince);
      break;
    case Screen::Ip: {
      char ip[16];
      WifiSetup::ipText(ip, sizeof(ip));
      Screens::ipAddress(ip, cfg.hostname, WifiSetup::staSsid());
      break;
    }
    case Screen::Off:
      break;                                     // black frame
    case Screen::Clock:
      Screens::clock(timeOk ? &local : nullptr, ms, cfg);
      break;
    case Screen::Plane:
      Screens::plane(s_havePlane ? &s_shownPlane : nullptr, s_shownCount);
      break;
    case Screen::Forecast:
      Screens::forecast(s.forecast.valid ? &s.forecast : nullptr);
      break;
    case Screen::Radar: {
      const uint8_t frames = s.radarFrames;
      const uint8_t f = radarFrame;
      Screens::radar(frames ? s.grids + f * RadarGrid::BYTES : nullptr, f, frames,
                     timeOk ? &local : nullptr, cfg);
      break;
    }
  }
  SkyData::unlock();

  Display::show();
}

void setMode(Mode m) {
  s_mode = m;
  s_rotSince = millis();                         // rotation starts with a full period
  DEBUG_LOG("STAT", "Mode: %s", modeName(s_mode));
  Settings::Values v = Settings::get();          // remember it over a restart
  if (v.screenMode != (uint8_t)m) {
    v.screenMode = (uint8_t)m;
    Settings::set(v);
  }
}

void cycleMode() {
  setMode((Mode)(((uint8_t)s_mode + 1) % MODE_COUNT));
}

Mode mode() { return s_mode; }

const char* modeName(Mode m) {
  switch (m) {
    case Mode::Auto:  return "AUTO";
    case Mode::Clock: return "CLOCK";
    case Mode::Plane: return "PLANE";
    case Mode::Radar: return "RADAR";
    case Mode::Forecast: return "FORECAST";
    case Mode::Rotate:   return "ROTATE";
  }
  return "?";
}

const char* screenNameNow() { return screenName(s_screen); }

bool nightNow() { return s_night; }

void refreshRadar() {
  DEBUG_LOG("STAT", "Radar refresh requested");
  Fetcher::requestRadar();
}

void printInfo() {
  struct tm t;
  if (TimeSync::now(&t)) {
    DEBUG_LOG("INFO", "Time %02d:%02d:%02d %02d.%02d.%04d",
              t.tm_hour, t.tm_min, t.tm_sec, t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  } else {
    DEBUG_LOG("INFO", "Time not set yet");
  }
  SkyData::State& s = SkyData::lock();
  DEBUG_LOG("INFO", "Mode %s, screen %s, WiFi %s, night %s", modeName(s_mode),
            screenName(s_screen),
            WifiSetup::state() == WifiSetup::State::Connected ? "OK" : "down", s_night ? "yes" : "no");
  if (s.planeNearby) {
    DEBUG_LOG("INFO", "Plane %s %s %.1f km", s.plane.callsign, s.plane.type, s.plane.distKm);
  } else {
    DEBUG_LOG("INFO", "No plane nearby");
  }
  DEBUG_LOG("INFO", "Radar %u frames, newest %s UTC, rain nearby %s (%.0f km, %u dBZ)",
            s.radarFrames, s.rain.timeUtc, s.rain.nearby ? "YES" : "no",
            s.rain.nearestKm, RadarPalette::levelToDbz(s.rain.maxLevel));
  if (s.forecast.valid) {
    DEBUG_LOG("INFO", "Forecast now %.1f C, today %.0f/%.0f C, tomorrow %.0f/%.0f C",
              s.forecast.temp, s.forecast.day[0].tMax, s.forecast.day[0].tMin,
              s.forecast.day[1].tMax, s.forecast.day[1].tMin);
  } else {
    DEBUG_LOG("INFO", "No forecast yet");
  }
  SkyData::unlock();
}

}  // namespace Station
