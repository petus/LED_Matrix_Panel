/**
 * @file    png_stream.h
 * @brief   Feed PNGdec straight from a network stream – no buffer for the
 *          whole file.
 *
 * PNGdec reads a file through read/seek callbacks. It reads forward in
 * pieces of up to PNG_FILE_BUF_SIZE (2 kB) and only ever seeks back to the
 * part of its last read it has not used yet (or to byte 8 right after the
 * header). So a small sliding window of the stream is enough: every read
 * throws away what lies before its start position and tops the window up
 * from the stream.
 *
 * Without PSRAM this matters: a radar PNG kept whole needs one big free
 * block of RAM, which is often not available next to an open HTTPS
 * connection.
 *
 * Usage:
 *   static PngStream::Source src;           // 4 kB, keep it off the stack
 *   src.reset(readFn, ctx, contentLength);
 *   PngStream::current = &src;
 *   png.open("", PngStream::openCb, PngStream::closeCb,
 *            PngStream::readCb, PngStream::seekCb, onLine);
 *
 * Plain C++, no Arduino types – also builds on a PC for testing.
 */
#pragma once

#include <stdint.h>
#include <string.h>
#include <PNGdec.h>

namespace PngStream {

  /// Read up to len bytes from the stream. Returns the number of bytes read,
  /// 0 or less on end of data, timeout or error.
  typedef int32_t (*ReadFn)(void* ctx, uint8_t* buf, int32_t len);

  /// Window size – must be at least PNG_FILE_BUF_SIZE.
  constexpr int32_t WINDOW = 4096;
  static_assert(WINDOW >= PNG_FILE_BUF_SIZE, "window smaller than PNGdec reads");

  struct Source {
    void reset(ReadFn f, void* c, int32_t s) {
      fn = f; ctx = c; size = s; winStart = 0; winLen = 0; error = false;
      seekBack = false; errorPos = 0;
    }
    ReadFn   fn       = nullptr;
    void*    ctx      = nullptr;
    int32_t  size     = 0;      // total file size (Content-Length)
    int32_t  winStart = 0;      // file offset of win[0]
    int32_t  winLen   = 0;      // valid bytes in win
    bool     error    = false;  // stream ended early or a seek went too far back
    bool     seekBack = false;  // error kind: true = seek before the window
    int32_t  errorPos = 0;      // file position where it happened
    uint8_t  win[WINDOW];
  };

  /// The source for the next open() – PNGdec's open callback has no user pointer.
  inline Source* current = nullptr;

  inline void* openCb(const char*, int32_t* fileSize) {
    *fileSize = current ? current->size : 0;
    return current;
  }

  inline void closeCb(void*) {}

  inline int32_t seekCb(PNGFILE* f, int32_t pos) {
    if (pos < 0) pos = 0;
    if (pos > f->iSize) pos = f->iSize;
    f->iPos = pos;
    return pos;
  }

  inline int32_t readCb(PNGFILE* f, uint8_t* buf, int32_t len) {
    Source* s = static_cast<Source*>(f->fHandle);
    if (!s || len <= 0) return 0;
    const int32_t pos = f->iPos;
    if (pos < s->winStart) {                                 // already thrown away
      s->error = true; s->seekBack = true; s->errorPos = pos;
      return 0;
    }

    const int32_t end = s->winStart + s->winLen;
    if (pos >= end) {
      // Forward jump past the window: read and drop the bytes in between.
      int32_t skip = pos - end;
      uint8_t tmp[64];
      while (skip > 0) {
        const int32_t n = s->fn(s->ctx, tmp, skip > (int32_t)sizeof(tmp) ? (int32_t)sizeof(tmp) : skip);
        if (n <= 0) { s->error = true; s->errorPos = pos - skip; return 0; }
        skip -= n;
      }
      s->winStart = pos;
      s->winLen   = 0;
    } else if (pos > s->winStart) {
      // Drop what lies before pos – PNGdec never goes back there.
      const int32_t drop = pos - s->winStart;
      memmove(s->win, s->win + drop, s->winLen - drop);
      s->winStart = pos;
      s->winLen  -= drop;
    }

    if (len > WINDOW) len = WINDOW;
    if (len > f->iSize - pos) len = f->iSize - pos;
    while (s->winLen < len) {
      const int32_t n = s->fn(s->ctx, s->win + s->winLen, len - s->winLen);
      if (n <= 0) { s->error = true; s->errorPos = s->winStart + s->winLen; break; }
      s->winLen += n;
    }
    const int32_t n = (s->winLen < len) ? s->winLen : len;
    memcpy(buf, s->win, n);
    f->iPos += n;
    return n;
  }

}  // namespace PngStream
