/**
 * @file    fetcher.cpp
 * @brief   Network task (see fetcher.h).
 */
#include "fetcher.h"
#include "wifi_setup.h"
#include "config.h"
#include "debug.h"
#include "net.h"
#include "adsb.h"
#include "chmu.h"
#include "rainviewer.h"
#include "forecast.h"
#include "sky_data.h"
#include "settings.h"

#include <Arduino.h>

namespace {

constexpr uint32_t    TASK_STACK = 16 * 1024;   // TLS handshake + JSON need a roomy stack
constexpr UBaseType_t TASK_PRIO  = 1;
constexpr BaseType_t  TASK_CORE  = 0;
constexpr uint32_t    NET_SETTLE_MS = 2000;

// When neither aircraft, radar nor forecast are enabled, the clock still needs a
// "Date" header now and then. Google's connectivity check answers with an
// empty 204 – the cheapest request there is.
constexpr const char* TIME_URL       = "https://www.google.com/generate_204";
constexpr uint32_t    TIME_PERIOD_MS = 30UL * 60UL * 1000UL;

volatile bool s_radarRequested    = true;       // first download right away
volatile bool s_forecastRequested = true;
uint8_t*      s_scratch        = nullptr;       // decode target before publishing

// Aircraft state – shared by the task loop and the radar's idle hook.
uint32_t s_lastPlane    = 0;
bool     s_firstPlane   = true;
bool     s_planeCleared = false;                // "off" published once

// Ask adsb.fi when it is time (or force). Also keeps the clock right through
// the HTTP Date header.
void pollPlane(bool force) {
  const Settings::Values cfg = Settings::get();
  if (!cfg.planeEnabled) {
    if (!s_planeCleared) {
      SkyData::publishPlane(false, Aircraft(), 0);
      s_planeCleared = true;
    }
    return;
  }
  const uint32_t now = millis();
  if (!force && !s_firstPlane && now - s_lastPlane < cfg.planePeriodS * 1000UL) return;
  s_firstPlane   = false;
  s_planeCleared = false;
  s_lastPlane    = now;
  ADSB::Filter f;
  f.radiusKm     = cfg.planeRadiusKm;
  f.minAltM      = cfg.planeMinAltM;
  f.callsignOnly = cfg.planeCallsignOnly;
  f.viewEnabled  = cfg.viewEnabled;
  f.viewDirDeg   = cfg.viewDirDeg;
  f.viewHalfDeg  = cfg.viewHalfDeg;
  f.viewRangeKm  = cfg.viewRangeKm;
  ADSB::Result r;
  if (ADSB::fetch(cfg.homeLat, cfg.homeLon, f, &r)) {
    SkyData::publishPlane(r.found, r.plane, r.count);
  }
}

// Called by the radar sources between frames (no connection open then).
void radarIdle() {
  if (Net::connected()) pollPlane(false);
}

void task(void*) {
  uint32_t lastRadar = 0, lastTime = 0;
  uint32_t radarWait = 0;
  uint32_t lastForecast = 0, forecastWait = 0;
  bool     firstTime = true;
  bool     wasOnline = false;
  bool     radarCleared = false;                 // "off" published once
  char     radarStamp[13] = "";   // newest frame on the panel ("" = none / must reload)
  uint8_t  radarSource = 0xFF;    // source of the frames on the panel

  for (;;) {
    const bool online = Net::connected();
    if (!online) { wasOnline = false; vTaskDelay(pdMS_TO_TICKS(500)); continue; }
    // Never while the set-up access point runs (not enough RAM for HTTPS).
    if (WifiSetup::apActive()) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
    // Give DHCP / DNS a moment after connecting – the very first request
    // right after "connected" often fails ("connection refused").
    if (WifiSetup::connectedForMs() < NET_SETTLE_MS) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
    if (!wasOnline) {                         // (re)connected: fetch everything now
      wasOnline = true;
      s_firstPlane = true;
      s_radarRequested = true;
      s_forecastRequested = true;
    }

    const Settings::Values cfg = Settings::get();
    const uint32_t now = millis();

    // Aircraft – also keeps the clock right through the HTTP Date header.
    pollPlane(false);

    // Radar – heavier (index + several PNGs), every 5 minutes.
    if (cfg.radarEnabled && (s_radarRequested || now - lastRadar >= radarWait)) {
      // A forced reload (changed settings) or another source: download all again.
      if (s_radarRequested || radarSource != cfg.radarSource) radarStamp[0] = '\0';
      s_radarRequested = false;
      radarCleared = false;
      lastRadar = millis();
      DEBUG_LOG("NET", "Radar start: free heap %lu B, largest block %lu B",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
      RainInfo rain;
      [[maybe_unused]] const uint32_t t0 = millis();
      const uint8_t  minLevel = RadarPalette::dbzToLevel(cfg.rainMinDbz);
      const char*    known    = radarStamp[0] ? radarStamp : nullptr;
      uint8_t frames;
      if (cfg.radarSource == RADAR_SRC_RAINVIEWER) {
        frames = RainViewer::fetch(s_scratch, cfg.radarFrames, cfg.homeLat, cfg.homeLon,
                                   cfg.rainRadiusKm, minLevel, &rain, known, radarIdle);
      } else {
        frames = CHMU::fetch(s_scratch, cfg.radarFrames, cfg.radarSource == RADAR_SRC_CHMU_MASKED,
                             cfg.homeLat, cfg.homeLon,
                             cfg.rainRadiusKm, minLevel, &rain, known, radarIdle);
      }
      if (frames == RADAR_UNCHANGED) {
        radarWait = RADAR_RETRY_MS;            // nothing new yet – ask again in a minute
      } else if (frames) {
        SkyData::publishRadar(s_scratch, frames, rain);
        memcpy(radarStamp, rain.timeUtc, sizeof(radarStamp));
        radarSource = cfg.radarSource;
        radarWait = RADAR_PERIOD_MS;
        DEBUG_LOG("NET", "Radar: %u frames in %lu ms (task stack free %u B)", frames,
                  (unsigned long)(millis() - t0), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
      } else {
        radarWait = RADAR_RETRY_MS;
      }
    } else if (!cfg.radarEnabled && !radarCleared) {
      SkyData::publishRadar(s_scratch, 0, RainInfo());
      radarCleared = true;
      radarStamp[0] = '\0';
    }

    // Forecast – one small request every 15 minutes.
    if (cfg.forecastEnabled && (s_forecastRequested || millis() - lastForecast >= forecastWait)) {
      s_forecastRequested = false;
      lastForecast = millis();
      Weather::Forecast f;
      if (Weather::fetch(cfg.homeLat, cfg.homeLon, &f)) {
        SkyData::publishForecast(f);
        forecastWait = FORECAST_PERIOD_MS;
      } else {
        forecastWait = FORECAST_RETRY_MS;
      }
    }

    // Clock only: with aircraft, radar and forecast all off nothing else
    // brings a "Date" header – ask Google now and then.
    if (!cfg.planeEnabled && !cfg.radarEnabled && !cfg.forecastEnabled &&
        (firstTime || millis() - lastTime >= TIME_PERIOD_MS)) {
      firstTime = false;
      lastTime  = millis();
      Net::touch(TIME_URL, "TIME");
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

}  // namespace

namespace Fetcher {

void begin() {
  // Big blocks first, while the heap is still in one piece.
  CHMU::begin();
  s_scratch = (uint8_t*)Net::allocLarge(RADAR_FRAMES_MAX * RadarGrid::BYTES);
  if (!s_scratch) {
    DEBUG_LOG("NET", "ERROR: no memory for radar scratch buffer");
    return;
  }
  xTaskCreatePinnedToCore(task, "fetcher", TASK_STACK, nullptr, TASK_PRIO, nullptr, TASK_CORE);
}

void requestRadar() {
  s_radarRequested = true;
}

void requestForecast() {
  s_forecastRequested = true;
}

}  // namespace Fetcher
