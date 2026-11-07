#pragma once

#include "RoboTypes.hpp"

/*
 * UTF-8 → UCS-4 — mirrors GRUB grub_utf8_process (charset.h).
 * Invalid / truncated sequences yield U+FFFD.
 */
constexpr RoboU32 ROBO_UNICODE_REPLACEMENT = 0xFFFDu;

/* Process one UTF-8 byte. At start: *code = 0, *count = 0.
 * Returns ROBO_FALSE on invalid byte; ROBO_TRUE when the byte was accepted.
 * A codepoint is complete when the call succeeds and *count == 0. */
inline RoboBool RoboUTF8Process(RoboU8 byte, RoboU32* code, RoboI32* count) {
  if (code == ROBO_NULL || count == ROBO_NULL) {
    return ROBO_FALSE;
  }
  if (*count != 0) {
    if ((byte & 0xC0u) != 0x80u) {
      *count = 0;
      *code = 0;
      return ROBO_FALSE;
    }
    *code = (*code << 6) | (byte & 0x3Fu);
    --(*count);
    /* Overlong. */
    if ((*count == 1 && *code <= 0x1Fu) || (*count == 2 && *code <= 0x0Fu)) {
      *code = 0;
      *count = 0;
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }

  if ((byte & 0x80u) == 0) {
    *code = byte;
    return ROBO_TRUE;
  }
  if ((byte & 0xE0u) == 0xC0u) {
    *count = 1;
    *code = byte & 0x1Fu;
    if (*code <= 1) {
      *count = 0;
      *code = 0;
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if ((byte & 0xF0u) == 0xE0u) {
    *count = 2;
    *code = byte & 0x0Fu;
    return ROBO_TRUE;
  }
  if ((byte & 0xF8u) == 0xF0u) {
    *count = 3;
    *code = byte & 0x07u;
    if (*code > 0x04u) {
      *count = 0;
      *code = 0;
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  return ROBO_FALSE;
}

/* Decode next codepoint from *utf8; advances *utf8. Returns 0 at end. */
inline RoboU32 RoboUTF8Next(const char** utf8) {
  if (utf8 == ROBO_NULL || *utf8 == ROBO_NULL || **utf8 == 0) {
    return 0;
  }
  RoboU32 code = 0;
  RoboI32 count = 0;
  const char* p = *utf8;
  while (*p != 0) {
    const RoboU8 byte = static_cast<RoboU8>(*p);
    ++p;
    if (!RoboUTF8Process(byte, &code, &count)) {
      *utf8 = p;
      return ROBO_UNICODE_REPLACEMENT;
    }
    if (count == 0) {
      *utf8 = p;
      if (code > 0x10FFFFu) {
        return ROBO_UNICODE_REPLACEMENT;
      }
      return code;
    }
  }
  *utf8 = p;
  if (count != 0) {
    return ROBO_UNICODE_REPLACEMENT;
  }
  return 0;
}

inline RoboBool RoboUTF8IsSpace(RoboU32 code) {
  return (code == 0x20u || code == 0x09u || code == 0x0Du || code == 0xA0u || code == 0x3000u) ? ROBO_TRUE
                                                                                               : ROBO_FALSE;
}
