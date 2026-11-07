#pragma once

#include "../RoboShared/RoboTypes.hpp"

constexpr RoboU32 ROBO_DAMAGE_MAX_RECTS = 64;

struct RoboDamageRect {
  RoboI32 x;
  RoboI32 y;
  RoboI32 width;
  RoboI32 height;
};

struct RoboDamageList {
  RoboDamageRect rects[ROBO_DAMAGE_MAX_RECTS];
  RoboU32 count;
  RoboBool full; /* overflow / Clear→full: Present entire FB (drm_fb_helper style) */
};

inline void RoboDamageClear(RoboDamageList* list) {
  if (list == ROBO_NULL) {
    return;
  }
  list->count = 0;
  list->full = ROBO_FALSE;
}

inline void RoboDamageMarkFull(RoboDamageList* list) {
  if (list == ROBO_NULL) {
    return;
  }
  list->full = ROBO_TRUE;
  list->count = 0;
}

inline RoboBool RoboDamageIsEmpty(const RoboDamageList* list) {
  if (list == ROBO_NULL) {
    return ROBO_TRUE;
  }
  return (!list->full && list->count == 0) ? ROBO_TRUE : ROBO_FALSE;
}

/* drm_fb_helper_memory_damage — expand one accumulating AABB when list is full. */
inline void RoboDamageAdd(RoboDamageList* list, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  if (list == ROBO_NULL || width <= 0 || height <= 0) {
    return;
  }
  if (list->full) {
    return;
  }
  if (list->count >= ROBO_DAMAGE_MAX_RECTS) {
    RoboDamageMarkFull(list);
    return;
  }
  list->rects[list->count++] = RoboDamageRect{x, y, width, height};
}

/*
 * drm_atomic_helper_damage_merged — single AABB covering all clips.
 * Returns ROBO_FALSE if the list is empty.
 */
inline RoboBool RoboDamageMerge(const RoboDamageList* list, RoboDamageRect* out) {
  if (list == ROBO_NULL || out == ROBO_NULL || RoboDamageIsEmpty(list)) {
    return ROBO_FALSE;
  }
  if (list->full) {
    out->x = 0;
    out->y = 0;
    out->width = 0x7fffffff;
    out->height = 0x7fffffff;
    return ROBO_TRUE;
  }
  RoboI32 x0 = list->rects[0].x;
  RoboI32 y0 = list->rects[0].y;
  RoboI32 x1 = list->rects[0].x + list->rects[0].width;
  RoboI32 y1 = list->rects[0].y + list->rects[0].height;
  for (RoboU32 i = 1; i < list->count; ++i) {
    const RoboDamageRect& r = list->rects[i];
    if (r.x < x0) {
      x0 = r.x;
    }
    if (r.y < y0) {
      y0 = r.y;
    }
    if (r.x + r.width > x1) {
      x1 = r.x + r.width;
    }
    if (r.y + r.height > y1) {
      y1 = r.y + r.height;
    }
  }
  out->x = x0;
  out->y = y0;
  out->width = x1 - x0;
  out->height = y1 - y0;
  return ROBO_TRUE;
}
