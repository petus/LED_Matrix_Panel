/**
 * @file    web.h
 * @brief   Web server: settings page + JSON API + firmware update.
 *
 *   GET  /                 settings page (web_page.h)
 *   GET  /api/status       time, screen, WiFi, aircraft, radar, memory
 *   GET  /api/config       all settings
 *   POST /api/config       change settings (JSON, only the keys sent)
 *   GET  /api/scan         WiFi networks around ({done:false} while scanning)
 *   POST /api/wifi         {ssid, pass} – connect to a home network
 *   POST /api/wifi/forget  forget the home network -> access point
 *   POST /api/mode         {mode: AUTO|CLOCK|PLANE|RADAR}
 *   GET  /api/export       settings as a downloadable file
 *   POST /api/import       settings from such a file
 *   POST /api/reboot       restart
 *   POST /api/reset        factory reset (settings + WiFi) and restart
 *   POST /update           firmware .bin (multipart upload)
 *
 * In access-point mode every unknown address is redirected to the page,
 * so phones open it automatically after joining the network.
 * Runs in the main loop (Web::loop()).
 */
#pragma once

namespace Web {

  void begin();
  void loop();

}  // namespace Web
