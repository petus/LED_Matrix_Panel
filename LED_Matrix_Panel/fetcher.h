/**
 * @file    fetcher.h
 * @brief   Background network task: aircraft every few seconds,
 *          ČHMÚ radar every RADAR_PERIOD_MS (settings from Settings). Results go to SkyData.
 *
 * Runs on core 0 (next to the WiFi stack); the display loop stays on core 1
 * and is never blocked by a slow download.
 */
#pragma once

namespace Fetcher {

  /// Start WiFi and the background task.
  void begin();

  /// Ask for a radar download as soon as possible (console command).
  void requestRadar();

  /// Ask for a new forecast as soon as possible (position changed).
  void requestForecast();

}  // namespace Fetcher
