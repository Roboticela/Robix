#ifndef RXU_RASTER_H
#define RXU_RASTER_H

#include <stdint.h>

/* Universal display-list rasterizer.
   RGBA8, origin top-left, premultiplied alpha.
   No OS headers and no OS font API. */

typedef struct RxuRaster {
    uint8_t *pixels;
    int width;
    int height;
    int dirty;
    uint64_t hash;
} RxuRaster;

/* text is the display list. damage_w == 0 rasters the whole buffer. */
int rxu_raster_text(const char *text, int damage_x, int damage_y, int damage_w, int damage_h, RxuRaster *out);

/* Front-most command under a device pixel. Returns 0 when the point misses. */
int rxu_raster_hit(const char *text, int x, int y, char *id, int id_cap, int *hit_x, int *hit_y, int *hit_w, int *hit_h);

/* Replace the content of one Text command. The caller frees the result. */
char *rxu_raster_set_text(const char *text, const char *id, const char *content);

void rxu_raster_free(RxuRaster *out);

#endif
