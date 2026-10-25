/**
 * @file    sky_data.cpp
 * @brief   Shared state with a FreeRTOS mutex (see sky_data.h).
 */
#include "sky_data.h"
#include "net.h"
#include "debug.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace {
SkyData::State    s_state;
SemaphoreHandle_t s_mutex = nullptr;
}

namespace SkyData {

bool begin() {
  s_mutex = xSemaphoreCreateMutex();
  s_state.grids = (uint8_t*)Net::allocLarge(RADAR_FRAMES_MAX * RadarGrid::BYTES);
  if (!s_mutex || !s_state.grids) {
    DEBUG_LOG("DATA", "ERROR: allocation failed");
    return false;
  }
  memset(s_state.grids, 0, RADAR_FRAMES_MAX * RadarGrid::BYTES);
  DEBUG_LOG("DATA", "Radar buffer %u B (%u frames)",
            (unsigned)(RADAR_FRAMES_MAX * RadarGrid::BYTES), RADAR_FRAMES_MAX);
  return true;
}

State& lock() {
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  return s_state;
}

void unlock() {
  xSemaphoreGive(s_mutex);
}

void publishPlane(bool nearby, const Aircraft& a, uint8_t count) {
  State& s = lock();
  s.planeValid     = true;
  s.planeNearby    = nearby;
  if (nearby) s.plane = a;
  s.planeCount     = nearby ? count : 0;
  s.planeUpdatedMs = millis();
  unlock();
}

void publishRadar(const uint8_t* grids, uint8_t frames, const RainInfo& rain) {
  State& s = lock();
  memcpy(s.grids, grids, frames * RadarGrid::BYTES);
  s.radarFrames = frames;
  s.rain        = rain;
  s.radarVersion++;
  unlock();
}

void publishForecast(const Weather::Forecast& f) {
  State& s = lock();
  s.forecast          = f;
  s.forecastUpdatedMs = millis();
  s.forecastVersion++;
  unlock();
}

}  // namespace SkyData
