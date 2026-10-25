/**
 * @file    sky_data.h
 * @brief   Data shared between the network task (writer) and the display
 *          loop (reader), protected by a mutex.
 *
 * Network task (core 0)  --publish*()-->  SkyData  --lock()/read-->  display (core 1)
 *
 * The display never waits for the network: downloads run in the background,
 * results appear here at once when complete.
 */
#pragma once

#include <stdint.h>
#include "adsb.h"
#include "chmu.h"
#include "forecast.h"
#include "config.h"

namespace SkyData {

  struct State {
    // Aircraft
    bool     planeValid   = false;    // at least one successful adsb.fi answer
    bool     planeNearby  = false;    // airborne aircraft within the set radius
    Aircraft plane;                   // the nearest one
    uint8_t  planeCount   = 0;        // aircraft in range, including plane
    uint32_t planeUpdatedMs = 0;

    // Radar
    uint8_t  radarFrames  = 0;        // valid frames in grids (oldest first)
    RainInfo rain;
    uint32_t radarVersion = 0;        // +1 with every new set of frames
    uint8_t* grids        = nullptr;  // RADAR_FRAMES_MAX * RadarGrid::BYTES

    // Forecast
    Weather::Forecast forecast;
    uint32_t forecastUpdatedMs = 0;
    uint32_t forecastVersion   = 0;   // +1 with every new forecast
  };

  /// Allocate buffers and the mutex. Call once before the network task starts.
  bool begin();

  /// Exclusive access for reading – keep the section short (one frame draw).
  State& lock();
  void   unlock();

  // Writers (network task)
  void publishPlane(bool nearby, const Aircraft& a, uint8_t count);
  void publishRadar(const uint8_t* grids, uint8_t frames, const RainInfo& rain);
  void publishForecast(const Weather::Forecast& f);

}  // namespace SkyData
