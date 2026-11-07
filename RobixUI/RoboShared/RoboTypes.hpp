#pragma once

#include <stdint.h>
#include <stddef.h>

using RoboU8 = uint8_t;
using RoboU16 = uint16_t;
using RoboU32 = uint32_t;
using RoboU64 = uint64_t;
using RoboI16 = int16_t;
using RoboI32 = int32_t;
using RoboI64 = int64_t;
using RoboSize = size_t;
using RoboBool = RoboU8;

constexpr RoboBool ROBO_TRUE = 1;
constexpr RoboBool ROBO_FALSE = 0;

#ifndef ROBO_NULL
#define ROBO_NULL nullptr
#endif

#ifndef ROBO_HOST
#if !defined(_MSC_VER)
inline void* operator new(size_t, void* ptr) noexcept {
  return ptr;
}
#endif
#endif

template <typename T>
constexpr T RoboMin(T a, T b) {
  return a < b ? a : b;
}

template <typename T>
constexpr T RoboMax(T a, T b) {
  return a > b ? a : b;
}

template <typename T>
constexpr T RoboClamp(T value, T low, T high) {
  return value < low ? low : (value > high ? high : value);
}

inline void RoboMemorySet(void* destination, RoboU8 value, RoboSize count) {
  auto* bytes = static_cast<RoboU8*>(destination);
  /* Wide fill when aligned and pattern is 0 (common clear path). */
  if (value == 0 && count >= 8 && (reinterpret_cast<RoboU64>(bytes) & 7u) == 0) {
    auto* qwords = reinterpret_cast<RoboU64*>(bytes);
    const RoboSize nq = count / 8;
    for (RoboSize i = 0; i < nq; ++i) {
      qwords[i] = 0;
    }
    bytes += nq * 8;
    count -= nq * 8;
  }
  for (RoboSize index = 0; index < count; ++index) {
    bytes[index] = value;
  }
}

/* Non-overlapping copy — drm_fb_memcpy / fbdev row blit hot path (64-bit words). */
inline void RoboMemoryCopy(void* destination, const void* source, RoboSize count) {
  auto* dst = static_cast<RoboU8*>(destination);
  const auto* src = static_cast<const RoboU8*>(source);
  while (count > 0 && ((reinterpret_cast<RoboU64>(dst) | reinterpret_cast<RoboU64>(src)) & 7u) != 0) {
    *dst++ = *src++;
    --count;
  }
  while (count >= 32) {
    reinterpret_cast<RoboU64*>(dst)[0] = reinterpret_cast<const RoboU64*>(src)[0];
    reinterpret_cast<RoboU64*>(dst)[1] = reinterpret_cast<const RoboU64*>(src)[1];
    reinterpret_cast<RoboU64*>(dst)[2] = reinterpret_cast<const RoboU64*>(src)[2];
    reinterpret_cast<RoboU64*>(dst)[3] = reinterpret_cast<const RoboU64*>(src)[3];
    dst += 32;
    src += 32;
    count -= 32;
  }
  while (count >= 8) {
    *reinterpret_cast<RoboU64*>(dst) = *reinterpret_cast<const RoboU64*>(src);
    dst += 8;
    src += 8;
    count -= 8;
  }
  while (count > 0) {
    *dst++ = *src++;
    --count;
  }
}

/* Overlap-safe move — fb_copyarea reverse/forward (same-buffer scroll). */
inline void RoboMemoryMove(void* destination, const void* source, RoboSize count) {
  auto* dst = static_cast<RoboU8*>(destination);
  const auto* src = static_cast<const RoboU8*>(source);
  if (dst == src || count == 0) {
    return;
  }
  if (dst < src || dst >= src + count) {
    RoboMemoryCopy(dst, src, count);
    return;
  }
  dst += count;
  src += count;
  while (count > 0) {
    --dst;
    --src;
    *dst = *src;
    --count;
  }
}

inline RoboI32 RoboMemoryCompare(const void* a, const void* b, RoboSize count) {
  const auto* bytesA = static_cast<const RoboU8*>(a);
  const auto* bytesB = static_cast<const RoboU8*>(b);
  if (bytesA == bytesB || count == 0) return 0;
  if (bytesA == ROBO_NULL) return -1;
  if (bytesB == ROBO_NULL) return 1;
  for (RoboSize i = 0; i < count; ++i) {
    if (bytesA[i] != bytesB[i]) {
      return static_cast<RoboI32>(bytesA[i]) - static_cast<RoboI32>(bytesB[i]);
    }
  }
  return 0;
}

inline RoboSize RoboStringLength(const char* text) {
  RoboSize length = 0;
  if (text == ROBO_NULL) {
    return 0;
  }
  while (text[length] != 0) {
    ++length;
  }
  return length;
}

inline RoboI32 RoboStringCompare(const char* a, const char* b) {
  if (a == b) return 0;
  if (a == ROBO_NULL) return -1;
  if (b == ROBO_NULL) return 1;
  while (*a != 0 && *a == *b) {
    ++a;
    ++b;
  }
  return static_cast<RoboI32>(static_cast<RoboU8>(*a)) - static_cast<RoboI32>(static_cast<RoboU8>(*b));
}

inline void RoboCopyString(char* destination, const char* source, RoboSize maxCount) {
  if (destination == ROBO_NULL || maxCount == 0) return;
  RoboSize i = 0;
  if (source != ROBO_NULL) {
    while (i + 1 < maxCount && source[i] != 0) {
      destination[i] = source[i];
      ++i;
    }
  }
  destination[i] = 0;
}
