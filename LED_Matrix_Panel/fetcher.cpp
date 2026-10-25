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

void task(void*) {
  uint32_t lastPlane = 0, lastRadar = 0, lastTime = 0;
  uint32_t radarWait = 0;
  uint32_t lastForecast = 0, forecastWait = 0;
  bool     firstPlane = true, firstTime = true;
  bool     wasOnline = false;
  bool     planeCleared = false, radarCleared = false;   // "off" published once

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
      firstPlane = true;
      s_radarRequested = true;
      s_forecastRequested = true;
    }

    const Settings::Values cfg = Settings::get();
    const uint32_t now = millis();

    // Aircraft – also keeps the clock right through the HTTP Date header.
    if (cfg.planeEnabled) {
      if (firstPlane || now - lastPlane >= cfg.planePeriodS * 1000UL) {
        firstPlane = false;
        planeCleared = false;
        lastPlane = now;
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
    } else if (!planeCleared) {
      SkyData::publishPlane(false, Aircraft(), 0);
      planeCleared = true;
    }

    // Radar – heavier (index + several PNGs), every 5 minutes.
    if (cfg.radarEnabled && (s_radarRequested || now - lastRadar >= radarWait)) {
      s_radarRequested = false;
      radarCleared = false;
      lastRadar = millis();
      DEBUG_LOG("NET", "Radar start: free heap %lu B, largest block %lu B",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
      RainInfo rain;
      [[maybe_unused]] const uint32_t t0 = millis();
      const uint8_t frames = CHMU::fetch(s_scratch, cfg.radarFrames, cfg.homeLat, cfg.homeLon,
                                         cfg.rainRadiusKm, RadarPalette::dbzToLevel(cfg.rainMinDbz),
                                         &rain);
      if (frames) {
        SkyData::publishRadar(s_scratch, frames, rain);
        radarWait = RADAR_PERIOD_MS;
        DEBUG_LOG("NET", "Radar: %u frames in %lu ms (task stack free %u B)", frames,
                  (unsigned long)(millis() - t0), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
      } else {
        radarWait = RADAR_RETRY_MS;
      }
    } else if (!cfg.radarEnabled && !radarCleared) {
      SkyData::publishRadar(s_scratch, 0, RainInfo());
      radarCleared = true;
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
