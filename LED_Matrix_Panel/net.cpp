/**
 * @file    net.cpp
 * @brief   HTTPS GET helpers (see net.h).
 */
#include "net.h"
#include "config.h"
#include "debug.h"
#include "time_sync.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <new>

namespace {

constexpr uint32_t HTTP_CONNECT_MS   = 8000;
constexpr uint32_t HTTP_TIMEOUT_MS   = 15000;
constexpr size_t   CHUNKED_START_SIZE = 4096;      // first buffer for answers without length

// -----------------------------------------------------------------------------
//  Stream sinks for HTTPClient::writeToStream() – it handles chunked
//  transfer encoding for us, the sinks only decide where the bytes go.
// -----------------------------------------------------------------------------

// Collects the body in a buffer. With a known Content-Length the buffer is
// allocated once at the right size; for chunked answers (no length) it starts
// small and grows as data arrives, up to maxBytes. Without PSRAM a large
// up-front reservation would often fail on a fragmented heap.
class BufferSink : public Stream {
public:
  BufferSink(size_t initial, size_t maxBytes) : _limit(maxBytes) { grow(initial); }
  ~BufferSink() { free(_buf); }
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    if (_failed) return 0;
    if (_len + len + 1 > _cap) {
      size_t want = _cap ? _cap * 2 : 1024;
      while (want < _len + len + 1) want *= 2;
      if (want > _limit + 1) want = _limit + 1;
      if (_len + len + 1 > want || !grow(want)) { _failed = true; return 0; }
    }
    memcpy(_buf + _len, data, len);
    _len += len;
    return len;
  }
  int  available() override { return 0; }
  int  read() override      { return -1; }
  int  peek() override      { return -1; }
  void flush() override     {}
  size_t length() const     { return _len; }
  bool failed() const       { return _failed || !_buf; }
  // Hand the buffer over to the caller (adds the terminating zero).
  uint8_t* release() { _buf[_len] = '\0'; uint8_t* b = _buf; _buf = nullptr; return b; }
private:
  bool grow(size_t cap) {
    void* p = heap_caps_realloc(_buf, cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = realloc(_buf, cap);
    if (!p) return false;
    _buf = (uint8_t*)p;
    _cap = cap;
    return true;
  }
  uint8_t* _buf    = nullptr;
  size_t   _cap    = 0;
  size_t   _len    = 0;
  size_t   _limit;
  bool     _failed = false;
};

// Passes the body to a callback in windows of WIN bytes. The last OVERLAP
// bytes of each window are repeated at the start of the next one, so a token
// (e.g. a file name) cut by the window edge is still seen whole once.
class ScanSink : public Stream {
public:
  ScanSink(Net::ScanFn fn, void* user) : _fn(fn), _user(user) {}
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    size_t done = 0;
    while (done < len) {
      size_t n = WIN - _fill;
      if (n > len - done) n = len - done;
      memcpy(_win + _fill, data + done, n);
      _fill += n; done += n; _total += n;
      if (_fill == WIN) flushWindow(true);
    }
    return len;
  }
  void finish() { if (_fill) flushWindow(false); }
  int  available() override { return 0; }
  int  read() override      { return -1; }
  int  peek() override      { return -1; }
  void flush() override     {}
  size_t total() const      { return _total; }
private:
  static constexpr size_t WIN     = 1024;
  static constexpr size_t OVERLAP = 96;
  void flushWindow(bool keepTail) {
    _win[_fill] = '\0';
    _fn(_win, _user);
    if (keepTail) {
      memmove(_win, _win + _fill - OVERLAP, OVERLAP);
      _fill = OVERLAP;
    } else {
      _fill = 0;
    }
  }
  Net::ScanFn _fn;
  void*       _user;
  char        _win[WIN + 1];
  size_t      _fill  = 0;
  size_t      _total = 0;
};

// Shared request set-up. Returns the HTTP status code (<0 = transport error).
int startGet(HTTPClient& http, WiFiClientSecure& client, const char* url, const char* tag) {
  client.setInsecure();                       // public data, no certificate pinning
  http.setConnectTimeout(HTTP_CONNECT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setReuse(false);
  if (!http.begin(client, url)) {
    DEBUG_LOG("NET", "%s: begin() failed", tag);
    return -1;
  }
  static const char* HEADERS[] = { "Date" };
  http.collectHeaders(HEADERS, 1);
  http.setUserAgent(HTTP_USER_AGENT);         // addHeader() would ignore User-Agent

  const int code = http.GET();
  if (http.hasHeader("Date")) TimeSync::setFromHttpDate(http.header("Date").c_str());
  if (code != HTTP_CODE_OK && code != 204) {
    DEBUG_LOG("NET", "%s: HTTP %d %s (free heap %lu B, largest block %lu B)", tag, code,
              code < 0 ? HTTPClient::errorToString(code).c_str() : "",
              (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  }
  return code;
}

}  // namespace

namespace Net {

bool connected() {
  return WiFi.status() == WL_CONNECTED;
}

void* allocLarge(size_t bytes) {
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(bytes);
  return p;
}

long getToBuffer(const char* url, uint8_t** buf, size_t maxBytes, const char* tag) {
  *buf = nullptr;
  if (!connected()) return -1;

  WiFiClientSecure client;
  HTTPClient http;
  if (startGet(http, client, url, tag) != HTTP_CODE_OK) { http.end(); return -1; }

  const int declared = http.getSize();        // -1 = chunked / unknown
  if (declared > (int)maxBytes) {
    DEBUG_LOG("NET", "%s: body %d B exceeds limit %u B", tag, declared, (unsigned)maxBytes);
    http.end();
    return -1;
  }

  BufferSink sink(declared > 0 ? (size_t)declared + 1 : CHUNKED_START_SIZE, maxBytes);
  const int ret = http.writeToStream(&sink);
  http.end();
  if (ret < 0 || sink.failed()) {
    DEBUG_LOG("NET", "%s: body read failed (%d, %u B, free heap %lu, largest %lu)",
              tag, ret, (unsigned)sink.length(),
              (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
    return -1;
  }
  const long len = (long)sink.length();
  *buf = sink.release();
  return len;
}

bool touch(const char* url, const char* tag) {
  if (!connected()) return false;
  WiFiClientSecure client;
  HTTPClient http;
  const int code = startGet(http, client, url, tag);
  http.end();
  return code > 0;
}

long getScan(const char* url, ScanFn fn, void* user, const char* tag) {
  if (!connected()) return -1;

  WiFiClientSecure client;
  HTTPClient http;
  if (startGet(http, client, url, tag) != HTTP_CODE_OK) { http.end(); return -1; }

  // The sink holds a 1 kB window – keep it off the network task's stack.
  ScanSink* sink = new (std::nothrow) ScanSink(fn, user);
  if (!sink) { http.end(); return -1; }
  const int ret = http.writeToStream(sink);
  sink->finish();
  const long total = (long)sink->total();
  delete sink;
  http.end();
  return ret < 0 ? -1 : total;
}

namespace {
// Read len bytes, waiting for them up to HTTP_TIMEOUT_MS. Asks available()
// first, the same way HTTPClient does: a TLS read with no decrypted data
// waiting may be taken as an error and close the connection, so readBytes()
// (byte-wise read() in a loop) can lose the rest of the body.
int32_t streamRead(void* stream, uint8_t* buf, int32_t len) {
  Client* c = static_cast<Client*>(stream);
  int32_t  got  = 0;
  uint32_t last = millis();
  while (got < len) {
    const int avail = c->available();
    if (avail > 0) {
      const int want = (avail < len - got) ? avail : (len - got);
      const int n = c->read(buf + got, want);
      if (n > 0) { got += n; last = millis(); continue; }
    } else if (!c->connected()) {
      break;                                   // closed and nothing left
    }
    if (millis() - last > HTTP_TIMEOUT_MS) break;
    delay(1);
  }
  return got;
}
}  // namespace

bool getStream(const char* url, BodyFn fn, void* user, const char* tag) {
  if (!connected()) return false;

  WiFiClientSecure client;
  HTTPClient http;
  if (startGet(http, client, url, tag) != HTTP_CODE_OK) { http.end(); return false; }

  const long size = http.getSize();
  if (size <= 0) {                             // chunked: raw stream would contain chunk headers
    DEBUG_LOG("NET", "%s: no Content-Length, cannot stream", tag);
    http.end();
    return false;
  }
  Client* stream = http.getStreamPtr();
  if (!stream) { http.end(); return false; }
  const bool ok = fn(streamRead, stream, size, user);
  http.end();
  return ok;
}

}  // namespace Net
