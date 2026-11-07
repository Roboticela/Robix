#pragma once

#include "RoboTypes.hpp"

/* Roboticela bitmap — GRUB bitmap analogue. Extension: .roboimg (ESP: .RBI) */
constexpr RoboU32 ROBO_IMAGE_MAGIC = 0x49424F52u; /* 'ROBI' little-endian */
constexpr RoboU32 ROBO_IMAGE_VERSION = 1;

constexpr RoboU32 ROBO_PIXEL_FORMAT_RGBA8 = 1;

constexpr RoboU32 ROBO_IMAGE_FLAG_HD = 1u << 0;
constexpr RoboU32 ROBO_IMAGE_FLAG_SUPERSAMPLED = 1u << 1;

struct RoboImageHeader {
  RoboU32 magic;
  RoboU32 version;
  RoboU32 width;
  RoboU32 height;
  RoboU32 stride;
  RoboU32 pixelFormat;
  RoboU32 flags;
  RoboU32 sourceDpi; /* cook sets 192 for HD splash */
};

struct RoboImageView {
  const RoboImageHeader* header;
  const RoboU8* pixels;
};

inline RoboBool RoboImageValidate(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < sizeof(RoboImageHeader)) {
    return ROBO_FALSE;
  }
  const auto* header = static_cast<const RoboImageHeader*>(data);
  if (header->magic != ROBO_IMAGE_MAGIC || header->version != ROBO_IMAGE_VERSION) {
    return ROBO_FALSE;
  }
  if (header->pixelFormat != ROBO_PIXEL_FORMAT_RGBA8) {
    return ROBO_FALSE;
  }
  if (header->width == 0 || header->height == 0 || header->stride < header->width * 4) {
    return ROBO_FALSE;
  }
  const RoboU64 needed = static_cast<RoboU64>(sizeof(RoboImageHeader)) +
                         static_cast<RoboU64>(header->stride) * header->height;
  return needed <= size ? ROBO_TRUE : ROBO_FALSE;
}

inline RoboImageView RoboImageMakeView(const void* data) {
  RoboImageView view{};
  view.header = static_cast<const RoboImageHeader*>(data);
  view.pixels = static_cast<const RoboU8*>(data) + sizeof(RoboImageHeader);
  return view;
}
