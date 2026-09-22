/** @file ui_surface.h Platform-independent framebuffer and dirty tracking. */

#ifndef SDL_PLAYER_UI_SURFACE_H
#define SDL_PLAYER_UI_SURFACE_H

#include "ui_types.h"

#include <stddef.h>

typedef struct {
    pixel_t *pixels;
    int width;
    int height;
    int stride;
    bool owns_pixels;
    bool has_dirty;
    UiRect dirty;
} UiSurface;

/** Allocate a tightly packed surface. */
bool ui_surface_create(UiSurface *surface, int width, int height);

/** Wrap a caller-owned buffer. */
bool ui_surface_wrap(UiSurface *surface, pixel_t *pixels,
                     int width, int height, int stride);

void ui_surface_destroy(UiSurface *surface);
void ui_surface_fill(UiSurface *surface, pixel_t color);

/**
 * Copy a source rectangle at (dst_x,dst_y). Clipping preserves the matching
 * source offset, unlike the old Display_draw implementation.
 */
bool ui_surface_blit(UiSurface *surface, const pixel_t *source,
                     int source_width, int source_height, int source_stride,
                     int dst_x, int dst_y);

void ui_surface_mark_dirty(UiSurface *surface, UiRect rect);
void ui_surface_mark_all_dirty(UiSurface *surface);
void ui_surface_clear_dirty(UiSurface *surface);
UiRect ui_surface_dirty_rect(const UiSurface *surface);

#endif
