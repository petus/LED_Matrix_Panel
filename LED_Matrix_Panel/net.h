/**
 * @file    net.h
 * @brief   Small HTTPS helpers (the WiFi itself is handled by wifi_setup).
 *
 * Every response's "Date" header is passed to TimeSync, so any successful
 * request also keeps the clock right.
 *
 * All functions here block – call them from the network task only
 * (fetcher.cpp), never from the display loop.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Net {

  /// True when the home WiFi is connected (internet may be reachable).
  bool connected();

  /// Allocate a large buffer – PSRAM when the board has it, internal RAM otherwise.
  void* allocLarge(size_t bytes);

  /// GET url and store the whole body in a newly allocated buffer (+ '\0').
  /// Answers without Content-Length are collected in a growing buffer.
  /// Returns body length, or -1 on error. Free *buf with free() afterwards.
  /// maxBytes limits the size (protects RAM against unexpected answers).
  long getToBuffer(const char* url, uint8_t** buf, size_t maxBytes, const char* tag);

  /// Request url only for its response header (sets the clock from "Date").
  /// Any status code counts as success. Returns false on transport error.
  bool touch(const char* url, const char* tag);

  /// Callback for getScan(): receives successive, overlapping text windows.
  typedef void (*ScanFn)(const char* text, void* user);

  /// GET url and feed the body to fn in windows without storing it all.
  /// Use for long listings (directory index). Returns total bytes or -1.
  long getScan(const char* url, ScanFn fn, void* user, const char* tag);

  /// Callback for getStream(): reads up to len body bytes into buf.
  /// Returns the number of bytes (0 = end of data / timeout).
  typedef int32_t (*ReadFn)(void* stream, uint8_t* buf, int32_t len);
  /// Callback for getStream(): consumes the body through read(stream, ...).
  typedef bool (*BodyFn)(ReadFn read, void* stream, long size, void* user);

  /// GET url and let fn read the body directly from the connection – nothing
  /// is buffered. Only for answers with a Content-Length (size > 0).
  /// Returns fn's result, false on HTTP error or unknown length.
  bool getStream(const char* url, BodyFn fn, void* user, const char* tag);

  /// One HTTPS connection kept open for several requests to the same host
  /// (keep-alive): the TLS handshake – the slow and memory-hungry part – is
  /// done once instead of for every radar tile / frame. Close it before
  /// talking to another host, so two TLS sessions never need RAM at once.
  struct Session;
  Session* openSession();
  void     closeSession(Session* s);         // nullptr is fine

  /// Results of fetchStream() besides HTTP status codes
  constexpr int FETCH_OK        = 200;
  constexpr int FETCH_BODY_FAIL = -1000;     // fn returned false / data ended early
  constexpr int FETCH_NO_LENGTH = -1001;     // answer without Content-Length

  /// Like getStream(), but over a session (nullptr = a one-off connection)
  /// and with the reason when it fails: FETCH_OK, an HTTP status such as 404,
  /// a negative transport error, FETCH_BODY_FAIL or FETCH_NO_LENGTH.
  int fetchStream(Session* s, const char* url, BodyFn fn, void* user, const char* tag);

}  // namespace Net
