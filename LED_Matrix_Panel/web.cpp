/**
 * @file    web.cpp
 * @brief   Web server and JSON API (see web.h).
 */
#include "web.h"
#include "web_page.h"
#include "config.h"
#include "debug.h"
#include "settings.h"
#include "station.h"
#include "sky_data.h"
#include "wifi_setup.h"
#include "time_sync.h"
#include "fetcher.h"
#include "radar_palette.h"

#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <string.h>
#include <math.h>

namespace {

WebServer s_srv(80);
uint32_t  s_rebootAt = 0;          // restart a moment after answering

void sendJson(const JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  s_srv.send(code, "application/json", out);
}

void sendError(const char* msg, int code = 400) {
  s_srv.send(code, "text/plain", msg);
}

// Parse the request body as JSON object. Sends an error and returns false on failure.
bool readBody(JsonDocument& doc) {
  if (!s_srv.hasArg("plain")) { sendError("missing body"); return false; }
  const String body = s_srv.arg("plain");
  if (deserializeJson(doc, body.c_str(), body.length()) || !doc.is<JsonObject>()) {
    sendError("invalid JSON");
    return false;
  }
  return true;
}

void sendConfig() {
  DEBUG_LOG("WEB", "Config requested");
  JsonDocument doc;
  Settings::toJson(Settings::get(), doc.to<JsonObject>());
  sendJson(doc);
}

// Apply settings from JSON and react to the ones that need more than a save.
void applyConfig(JsonObjectConst json) {
  const Settings::Values before = Settings::get();
  Settings::Values v = before;
  Settings::fromJson(json, v);
  Settings::set(v);

  if (strcmp(before.timeZone, v.timeZone) != 0) TimeSync::begin(v.timeZone);
  // Anything that changes what the radar looks for -> download again
  if (before.homeLat != v.homeLat || before.homeLon != v.homeLon ||
      before.rainRadiusKm != v.rainRadiusKm || before.rainMinDbz != v.rainMinDbz ||
      before.radarFrames != v.radarFrames || before.radarSource != v.radarSource ||
      (!before.radarEnabled && v.radarEnabled)) {
    Fetcher::requestRadar();
  }
  if (before.homeLat != v.homeLat || before.homeLon != v.homeLon ||
      (!before.forecastEnabled && v.forecastEnabled)) {
    Fetcher::requestForecast();
  }
}

// -----------------------------------------------------------------------------
//  Handlers
// -----------------------------------------------------------------------------
void handlePage() {
  DEBUG_LOG("WEB", "Page requested (host %s)", s_srv.hostHeader().c_str());
  s_srv.sendHeader("Cache-Control", "no-store");
  [[maybe_unused]] const uint32_t t0 = millis();
  s_srv.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  DEBUG_LOG("WEB", "Page sent (%u B) in %lu ms", (unsigned)strlen(WEB_PAGE),
            (unsigned long)(millis() - t0));
}

void handleStatus() {
  JsonDocument doc;
  struct tm t;
  if (TimeSync::now(&t)) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    doc["time"] = buf;
    snprintf(buf, sizeof(buf), "%d.%d.%d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    doc["date"] = buf;
  }
  doc["screen"] = Station::screenNameNow();
  doc["mode"]   = Station::modeName(Station::mode());
  doc["night"]  = Station::nightNow();
  doc["fw"]     = FW_NAME " " FW_VERSION;
  doc["heap"]    = ESP.getFreeHeap();
  doc["heapMax"] = ESP.getMaxAllocHeap();
  doc["uptime"]  = millis() / 1000;

  JsonObject w = doc["wifi"].to<JsonObject>();
  const bool connected = WifiSetup::state() == WifiSetup::State::Connected;
  char ip[16];
  WifiSetup::ipText(ip, sizeof(ip));
  w["connected"] = connected;
  w["ssid"]      = WifiSetup::staSsid();
  w["ip"]        = ip;
  w["rssi"]      = connected ? WifiSetup::rssi() : 0;
  w["ap"]        = WifiSetup::apActive();
  w["apSsid"]    = WifiSetup::apSsid();

  const Settings::Values cfg = Settings::get();
  SkyData::State& s = SkyData::lock();
  JsonObject p = doc["plane"].to<JsonObject>();
  p["on"]     = cfg.planeEnabled;
  p["nearby"] = s.planeNearby;
  if (s.planeNearby) {
    p["callsign"] = s.plane.callsign;
    p["reg"]      = s.plane.reg;
    p["hex"]      = s.plane.hex;
    p["type"]     = s.plane.type;
    p["distKm"]   = s.plane.distKm;
    p["altM"]     = (int)s.plane.altM;
    p["speedKmh"] = (int)s.plane.speedKmh;
    p["count"]    = s.planeCount;
  }
  JsonObject r = doc["radar"].to<JsonObject>();
  r["on"]     = cfg.radarEnabled;
  r["frames"] = s.radarFrames;
  r["src"]    = s.rain.source;                 // RADAR_SRC_* of the frames shown
  if (s.rain.timeUtc[0]) {                    // YYYYMMDDHHMM -> "HH:MM UTC"
    char buf[12];
    snprintf(buf, sizeof(buf), "%.2s:%.2s UTC", s.rain.timeUtc + 8, s.rain.timeUtc + 10);
    r["newest"] = buf;
  }
  r["nearby"]    = s.rain.nearby;
  r["nearestKm"] = (int)s.rain.nearestKm;
  r["maxDbz"]    = RadarPalette::levelToDbz(s.rain.maxLevel);
  JsonObject f = doc["forecast"].to<JsonObject>();
  f["on"] = cfg.forecastEnabled;
  if (s.forecast.valid) {
    f["temp"]   = roundf(s.forecast.temp * 10) / 10;
    f["code"]   = s.forecast.code;
    f["wind"]   = roundf(s.forecast.windMs * 10) / 10;
    f["todayMax"]    = (int)lroundf(s.forecast.day[0].tMax);
    f["todayMin"]    = (int)lroundf(s.forecast.day[0].tMin);
    f["tomorrowMax"] = (int)lroundf(s.forecast.day[1].tMax);
    f["tomorrowMin"] = (int)lroundf(s.forecast.day[1].tMin);
  }
  SkyData::unlock();

  sendJson(doc);
}

void handleConfigPost() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  applyConfig(doc.as<JsonObjectConst>());
  sendConfig();
}

void handleScan() {
  JsonDocument doc;
  doc["done"] = WifiSetup::scanResults(doc["networks"].to<JsonArray>());
  sendJson(doc);
}

void handleWifi() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (!ssid[0] || strlen(ssid) > 32 || strlen(pass) > 64) { sendError("invalid SSID or password"); return; }
  s_srv.send(200, "application/json", "{\"ok\":true}");
  WifiSetup::connectTo(ssid, pass);
}

void handleWifiForget() {
  s_srv.send(200, "application/json", "{\"ok\":true}");
  WifiSetup::forget();
}

void handleMode() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  const char* m = doc["mode"] | "";
  if      (!strcmp(m, "AUTO"))  Station::setMode(Station::Mode::Auto);
  else if (!strcmp(m, "CLOCK")) Station::setMode(Station::Mode::Clock);
  else if (!strcmp(m, "PLANE")) Station::setMode(Station::Mode::Plane);
  else if (!strcmp(m, "RADAR")) Station::setMode(Station::Mode::Radar);
  else if (!strcmp(m, "FORECAST")) Station::setMode(Station::Mode::Forecast);
  else if (!strcmp(m, "ROTATE"))   Station::setMode(Station::Mode::Rotate);
  else { sendError("unknown mode"); return; }
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

void handleExport() {
  JsonDocument doc;
  doc["device"]  = FW_NAME;
  doc["version"] = FW_VERSION;
  Settings::toJson(Settings::get(), doc["settings"].to<JsonObject>());
  String out;
  serializeJsonPretty(doc, out);
  s_srv.sendHeader("Content-Disposition", "attachment; filename=\"led-matrix-nastaveni.json\"");
  s_srv.send(200, "application/json", out);
}

void handleImport() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  // Accept both the export format {settings:{...}} and a bare settings object
  JsonObjectConst src = doc["settings"].is<JsonObjectConst>() ? doc["settings"].as<JsonObjectConst>()
                                                               : doc.as<JsonObjectConst>();
  applyConfig(src);
  sendConfig();
}

void handleReboot() {
  s_srv.send(200, "application/json", "{\"ok\":true}");
  s_rebootAt = millis() + 500;
}

void handleReset() {
  Settings::factoryReset();
  s_srv.send(200, "application/json", "{\"ok\":true}");
  s_rebootAt = millis() + 500;
}

// Firmware upload: called repeatedly with chunks, then handleUpdateDone().
void handleUpdateUpload() {
  HTTPUpload& up = s_srv.upload();
  if (up.status == UPLOAD_FILE_START) {
    DEBUG_LOG("WEB", "Firmware update: %s", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) DEBUG_LOG("WEB", "Update.begin failed: %s", Update.errorString());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      DEBUG_LOG("WEB", "Update.write failed: %s", Update.errorString());
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) DEBUG_LOG("WEB", "Firmware OK, %u B", up.totalSize);
    else                  DEBUG_LOG("WEB", "Update.end failed: %s", Update.errorString());
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleUpdateDone() {
  if (Update.hasError()) {
    sendError(Update.errorString(), 500);
  } else {
    s_srv.send(200, "text/plain", "OK");
    s_rebootAt = millis() + 1000;
  }
}

// Captive portal: in access-point mode every unknown address (the phone's
// connectivity checks like /generate_204 or /hotspot-detect.html) is sent
// to the settings page – that makes the "sign in to network" window appear.
void handleNotFound() {
  DEBUG_LOG("WEB", "%s%s -> %s", s_srv.hostHeader().c_str(), s_srv.uri().c_str(),
            WifiSetup::apActive() ? "redirect" : "404");
  if (WifiSetup::apActive()) {
    s_srv.sendHeader("Location", String("http://") + AP_IP + "/", true);
    s_srv.send(302, "text/plain", "");
    return;
  }
  s_srv.send(404, "text/plain", "Not found");
}

}  // namespace

namespace Web {

void begin() {
  s_srv.on("/",                HTTP_GET,  handlePage);
  s_srv.on("/api/status",      HTTP_GET,  handleStatus);
  s_srv.on("/api/config",      HTTP_GET,  sendConfig);
  s_srv.on("/api/config",      HTTP_POST, handleConfigPost);
  s_srv.on("/api/scan",        HTTP_GET,  handleScan);
  s_srv.on("/api/wifi",        HTTP_POST, handleWifi);
  s_srv.on("/api/wifi/forget", HTTP_POST, handleWifiForget);
  s_srv.on("/api/mode",        HTTP_POST, handleMode);
  s_srv.on("/api/export",      HTTP_GET,  handleExport);
  s_srv.on("/api/import",      HTTP_POST, handleImport);
  s_srv.on("/api/reboot",      HTTP_POST, handleReboot);
  s_srv.on("/api/reset",       HTTP_POST, handleReset);
  s_srv.on("/update",          HTTP_POST, handleUpdateDone, handleUpdateUpload);
  s_srv.onNotFound(handleNotFound);
  s_srv.begin();
  DEBUG_LOG("WEB", "Web server on port 80");
}

void loop() {
  s_srv.handleClient();
  if (s_rebootAt && (int32_t)(millis() - s_rebootAt) > 0) {
    DEBUG_LOG("WEB", "Restarting");
    delay(100);
    ESP.restart();
  }
}

}  // namespace Web
