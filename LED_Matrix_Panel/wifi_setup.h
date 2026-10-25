/**
 * @file    wifi_setup.h
 * @brief   WiFi connection with an access point for the first set-up.
 *
 *   no WiFi stored ───────────────► access point "LED-Matrix-XXXX" (open)
 *   WiFi stored ──► connecting ──► connected  (IP shown for 10 s, then normal)
 *                       │ 20 s
 *                       └────────► access point (one more try every 5 min
 *                                  while no phone is connected)
 *
 * The radio is either a station OR an access point, never both: in AP+STA
 * mode phones on the set-up network lose packets. After a network is
 * entered on the page the access point closes and the panel connects;
 * if that fails (wrong password...) the access point comes back.
 *
 * While the access point is up, a DNS server answers every name with the
 * panel's address, so phones open the settings page by themselves
 * (captive portal).
 *
 * Runs from the main loop (non-blocking).
 */
#pragma once

#include <stdint.h>
#include <ArduinoJson.h>

namespace WifiSetup {

  enum class State : uint8_t {
    Connecting,     // trying the stored home WiFi
    Connected,      // home WiFi up
    AccessPoint,    // set-up network up
  };

  void  begin();
  void  loop();

  State state();
  bool  apActive();
  const char* apSsid();           // "LED-Matrix-1A2B"
  const char* staSsid();          // stored home network
  void  ipText(char* out, size_t cap);   // address in the home network
  int   rssi();

  /// Milliseconds since the home WiFi connected (0 = not connected).
  uint32_t connectedForMs();

  /// Save new credentials and connect (closes the access point).
  void connectTo(const char* ssid, const char* pass);

  /// Forget the home WiFi and go back to the access point.
  void forget();

  /// Network scan for the web page. Returns false while still scanning.
  bool scanResults(JsonArray out);

}  // namespace WifiSetup
