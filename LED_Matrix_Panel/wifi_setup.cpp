/**
 * @file    wifi_setup.cpp
 * @brief   Home WiFi + set-up access point (see wifi_setup.h).
 *
 * The radio is always in ONE role: either station (home WiFi) or access
 * point. Never both at once – in AP+STA mode the station part searches
 * for its network on other channels and the phone on the access point
 * keeps losing packets (unstable set-up network, page never loads).
 * The same approach as in MeteoPlaneRadar.
 */
#include "wifi_setup.h"
#include "config.h"
#include "debug.h"
#include "settings.h"

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <string.h>

namespace {

constexpr uint8_t  DNS_PORT          = 53;
constexpr uint32_t SWITCH_DELAY_MS   = 1500;   // let the answer reach the phone first
constexpr uint32_t RECONNECT_MS      = 15000;  // lost home WiFi: try again this often

WifiSetup::State s_state = WifiSetup::State::Connecting;
DNSServer        s_dns;
bool             s_apOn        = false;
char             s_apSsid[24]  = "";
char             s_ssid[33]    = "";
char             s_pass[65]    = "";
uint32_t         s_since       = 0;      // start of the current state
uint32_t         s_connectedAt = 0;      // 0 = not connected
uint32_t         s_lastTry     = 0;      // last reconnect after a lost connection
uint32_t         s_switchAt    = 0;      // != 0: leave the AP and connect at this time
bool             s_everConnected = false;
bool             s_mdnsOn      = false;
bool             s_scanning    = false;
uint32_t         s_apClientAt  = 0;      // last time a phone was on the set-up network
uint32_t         s_apLogAt     = 0;      // last "AP alive" log line

// Log phones joining / leaving the set-up network (diagnostics).
void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      s_apClientAt = millis();
      DEBUG_LOG("WIFI", "Phone joined the set-up network");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      DEBUG_LOG("WIFI", "Phone left the set-up network");
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      DEBUG_LOG("WIFI", "Phone got address %s", IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
      break;
    default:
      break;
  }
}

void setState(WifiSetup::State st) {
  s_state = st;
  s_since = millis();
}

// What the radio really does as an access point – for the log.
void logApConfig() {
  wifi_config_t conf = {};
  wifi_mode_t   mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK) {
    int8_t power = 0;
    esp_wifi_get_max_tx_power(&power);                     // in 0.25 dBm
    DEBUG_LOG("WIFI", "AP config: SSID \"%.32s\", channel %u, auth %d, hidden %u, mode %d, TX %.1f dBm",
              (const char*)conf.ap.ssid, conf.ap.channel, (int)conf.ap.authmode,
              conf.ap.ssid_hidden, (int)mode, power * 0.25f);
  } else {
    DEBUG_LOG("WIFI", "AP config: not available (mode %d)", (int)mode);
  }
}

void startAp() {
  if (s_apOn) return;
  // Same order as MeteoPlaneRadar. Turn the station off only when it runs –
  // switching a radio that is not started off and on again is not needed.
  if (WiFi.getMode() & WIFI_MODE_STA) WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  // Open network, default address 192.168.4.1, fixed channel, visible.
  bool ok = WiFi.softAP(s_apSsid, nullptr, WIFI_AP_CHANNEL, 0, 4);
  if (!ok) {
    DEBUG_LOG("WIFI", "softAP() failed, restarting the radio");
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    ok = WiFi.softAP(s_apSsid, nullptr, WIFI_AP_CHANNEL, 0, 4);
  }
  delay(200);                             // give the AP a moment before DNS binds
  WiFi.setSleep(false);                   // the AP must send its beacons all the time
  WiFi.setTxPower(WIFI_AP_TX_POWER);      // lower current peaks (see config.h)
  // Every name -> us, so phones open the settings page (captive portal).
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(DNS_PORT, "*", WiFi.softAPIP());
  s_apOn = true;
  s_apClientAt = millis();
  s_apLogAt    = millis();
  setState(WifiSetup::State::AccessPoint);
  DEBUG_LOG("WIFI", "Access point \"%s\" at %s%s", s_apSsid, WiFi.softAPIP().toString().c_str(),
            ok ? "" : " (softAP() reported an error)");
  logApConfig();
}

void stopAp() {
  if (!s_apOn) return;
  s_dns.stop();
  WiFi.softAPdisconnect(true);
  s_apOn = false;
  DEBUG_LOG("WIFI", "Access point off");
}

// Station only: close the access point and try the home WiFi.
void startSta() {
  if (!s_ssid[0]) return;
  stopAp();
  WiFi.mode(WIFI_STA);
  // No modem sleep: with power saving the radio sleeps between beacons and
  // large downloads (radar index, PNG frames, tiles) crawl at a few kB/s.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  DEBUG_LOG("WIFI", "Connecting to \"%s\"", s_ssid);
  WiFi.begin(s_ssid, s_pass);
  setState(WifiSetup::State::Connecting);
}

void onConnected() {
  setState(WifiSetup::State::Connected);
  s_connectedAt   = millis();
  s_everConnected = true;
  DEBUG_LOG("WIFI", "Connected to \"%s\", IP %s, RSSI %d dBm",
            s_ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
  DEBUG_LOG("WIFI", "Free heap %lu B, largest block %lu B",
            (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());

  const Settings::Values cfg = Settings::get();
  if (!s_mdnsOn && MDNS.begin(cfg.hostname)) {
    MDNS.addService("http", "tcp", 80);
    s_mdnsOn = true;
    DEBUG_LOG("WIFI", "mDNS: http://%s.local", cfg.hostname);
  }
}

}  // namespace

namespace WifiSetup {

void begin() {
  // AP name ends with the last 4 hex digits of the MAC – unique per board.
  // Read from eFuse: WiFi.macAddress() returns zeros before WiFi is started.
  const uint64_t mac = ESP.getEfuseMac();             // byte 0 = first MAC byte
  snprintf(s_apSsid, sizeof(s_apSsid), "%s%02X%02X", AP_SSID_PREFIX,
           (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));

  WiFi.onEvent(onWifiEvent);
  WiFi.persistent(false);                  // credentials live in our own settings
  WiFi.setHostname(Settings::get().hostname);

  Settings::wifi(s_ssid, sizeof(s_ssid), s_pass, sizeof(s_pass));
  if (s_ssid[0]) {
    // Forget any network an older sketch stored in the WiFi driver itself –
    // otherwise the station would quietly try to join it on its own.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    startSta();
  } else {
    // No network: straight to the access point, like MeteoPlaneRadar (no
    // station start / stop right before it).
    DEBUG_LOG("WIFI", "No WiFi stored");
    startAp();
  }
}

void loop() {
  if (s_apOn) s_dns.processNextRequest();
  const bool up = WiFi.status() == WL_CONNECTED;
  const uint32_t now = millis();

  switch (s_state) {
    case State::Connecting:
      if (up) {
        onConnected();
      } else if (s_everConnected) {
        // Worked before (router restart, weak signal): keep trying, no AP.
        if (now - s_lastTry > RECONNECT_MS) {
          s_lastTry = now;
          DEBUG_LOG("WIFI", "Reconnecting");
          WiFi.reconnect();
        }
      } else if (now - s_since > WIFI_CONNECT_TIMEOUT_MS) {
        DEBUG_LOG("WIFI", "Home WiFi \"%s\" not reachable", s_ssid);
        startAp();
      }
      break;

    case State::AccessPoint:
      if (WiFi.softAPgetStationNum() > 0) s_apClientAt = now;
      if (now - s_apLogAt >= 60000) {
        s_apLogAt = now;
        DEBUG_LOG("WIFI", "Set-up network \"%s\" on, %u phone(s), channel %u, free heap %lu B",
                  s_apSsid, (unsigned)WiFi.softAPgetStationNum(), (unsigned)WiFi.channel(),
                  (unsigned long)ESP.getFreeHeap());
      }
      if (s_switchAt && (int32_t)(now - s_switchAt) >= 0) {
        s_switchAt = 0;                       // new network entered on the page
        startSta();
      } else if (!s_switchAt && now - s_apClientAt > WIFI_AP_RESTART_MS) {
        DEBUG_LOG("WIFI", "Nobody on the set-up network for %lu s, restarting it",
                  (unsigned long)(WIFI_AP_RESTART_MS / 1000));
        stopAp();
        WiFi.mode(WIFI_OFF);
        delay(100);
        startAp();
      } else if (s_ssid[0] && now - s_since > WIFI_AP_RETRY_MS &&
                 WiFi.softAPgetStationNum() == 0) {
        DEBUG_LOG("WIFI", "Trying the home WiFi again");
        startSta();
      }
      break;

    case State::Connected:
      if (!up) {
        s_connectedAt = 0;
        s_lastTry = now;
        setState(State::Connecting);
        DEBUG_LOG("WIFI", "Connection lost");
      }
      break;
  }
}

State state()          { return s_state; }
bool  apActive()       { return s_apOn; }
const char* apSsid()   { return s_apSsid; }
const char* staSsid()  { return s_ssid; }
int   rssi()           { return WiFi.RSSI(); }

void ipText(char* out, size_t cap) {
  snprintf(out, cap, "%s", WiFi.localIP().toString().c_str());
}

uint32_t connectedForMs() {
  return (s_state == State::Connected && s_connectedAt) ? millis() - s_connectedAt : 0;
}

void connectTo(const char* ssid, const char* pass) {
  // The same request twice in a row (double tap, browser retry) is ignored.
  const bool same = strcmp(ssid, s_ssid) == 0 && strcmp(pass, s_pass) == 0;
  if (same && (s_switchAt || s_state != State::AccessPoint)) {
    DEBUG_LOG("WIFI", "Already connecting to \"%s\", request ignored", ssid);
    return;
  }
  snprintf(s_ssid, sizeof(s_ssid), "%s", ssid);
  snprintf(s_pass, sizeof(s_pass), "%s", pass);
  Settings::setWifi(s_ssid, s_pass);
  DEBUG_LOG("WIFI", "New network \"%s\" saved", s_ssid);
  s_everConnected = false;                 // wrong password -> back to the AP
  if (s_apOn) {
    // The web answer must reach the phone before the access point closes.
    s_switchAt = millis() + SWITCH_DELAY_MS;
  } else {
    WiFi.disconnect();
    startSta();
  }
}

void forget() {
  Settings::clearWifi();
  s_ssid[0] = s_pass[0] = '\0';
  s_connectedAt   = 0;
  s_everConnected = false;
  startAp();
}

bool scanResults(JsonArray out) {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  if (n < 0 || !s_scanning) {               // not started yet (or failed) -> start
    WiFi.scanNetworks(true);
    s_scanning = true;
    return false;
  }
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = out.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  s_scanning = false;
  return true;
}

}  // namespace WifiSetup
