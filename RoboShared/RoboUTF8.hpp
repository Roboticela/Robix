#pragma once

#include "../RobixUI/Foundation/FoundationPlatform.hpp"

#include <cstring>

inline RoboSize RoboStringLength(const char* text) {
  return text == ROBO_NULL ? 0 : std::strlen(text);
}

inline void RoboMemorySet(void* memory, int value, RoboSize bytes) {
  if (memory != ROBO_NULL && bytes > 0) {
    std::memset(memory, value, bytes);
  }
}

inline RoboU32 RoboUTF8Next(const char** cursor) {
  if (cursor == ROBO_NULL || *cursor == ROBO_NULL) {
    return 0;
  }
  const auto* bytes = reinterpret_cast<const unsigned char*>(*cursor);
  if (bytes[0] == 0) {
    return 0;
  }
  if (bytes[0] < 0x80u) {
    *cursor += 1;
    return bytes[0];
  }
  if ((bytes[0] & 0xE0u) == 0xC0u && bytes[1] != 0) {
    *cursor += 2;
    return (static_cast<RoboU32>(bytes[0] & 0x1Fu) << 6) | (bytes[1] & 0x3Fu);
  }
  if ((bytes[0] & 0xF0u) == 0xE0u && bytes[1] != 0 && bytes[2] != 0) {
    *cursor += 3;
    return (static_cast<RoboU32>(bytes[0] & 0x0Fu) << 12) | (static_cast<RoboU32>(bytes[1] & 0x3Fu) << 6) |
           (bytes[2] & 0x3Fu);
  }
  if ((bytes[0] & 0xF8u) == 0xF0u && bytes[1] != 0 && bytes[2] != 0 && bytes[3] != 0) {
    *cursor += 4;
    return (static_cast<RoboU32>(bytes[0] & 0x07u) << 18) | (static_cast<RoboU32>(bytes[1] & 0x3Fu) << 12) |
           (static_cast<RoboU32>(bytes[2] & 0x3Fu) << 6) | (bytes[3] & 0x3Fu);
  }
  *cursor += 1;
  return bytes[0];
}

inline RoboBool RoboUTF8IsSpace(RoboU32 code) {
  return code == ' ' || code == '\t' || code == '\n' || code == '\r' || code == 0xA0u ? ROBO_TRUE : ROBO_FALSE;
}
