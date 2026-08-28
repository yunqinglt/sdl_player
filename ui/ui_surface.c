#include "ui_surface.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool dimensions_are_valid(int width, int height, int stride)
{
    return width > 0 && height > 0 && stride >= width;
}

bool ui_surface_create(UiSurface *surface, int width, int height)
{
    size_t pixel_count;

    if (surface == NULL || !dimensions_are_valid(width, height, width)) {
        return false;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return false;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / sizeof(pixel_t)) {
        return false;
    }

    memset(surface, 0, sizeof(*surface));
    surface->pixels = calloc(pixel_count, sizeof(pixel_t));
    if (surface->pixels == NULL) {
        return false;
    }

    surface->width = width;
    surface->height = height;
    surface->stride = width;
    surface->owns_pixels = true;
    ui_surface_mark_all_dirty(surface);
    return true;
}

bool ui_surface_wrap(UiSurface *surface, pixel_t *pixels,
                     int width, int height, int stride)
{
    if (surface == NULL || pixels == NULL ||
        !dimensions_are_valid(width, height, stride)) {
        return false;
    }

    memset(surface, 0, sizeof(*surface));
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
    surface->owns_pixels = false;
    ui_surface_mark_all_dirty(surface);
    return true;
}

void ui_surface_destroy(UiSurface *surface)
{
    if (surface == NULL) return;
    if (surface->owns_pixels) free(surface->pixels);
    memset(surface, 0, sizeof(*surface));
}

void ui_surface_fill(UiSurface *surface, pixel_t color)
{
    if (surface == NULL || surface->pixels == NULL) return;

    for (int y = 0; y < surface->height; ++y) {
        pixel_t *row = surface->pixels + (size_t)y * (size_t)surface->stride;
        for (int x = 0; x < surface->width; ++x) row[x] = color;
    }
    ui_surface_mark_all_dirty(surface);
}

bool ui_surface_blit(UiSurface *surface, const pixel_t *source,
                     int source_width, int source_height, int source_stride,
                     int dst_x, int dst_y)
{
    int source_x = 0;
    int source_y = 0;
    int copy_width = source_width;
    int copy_height = source_height;

    if (surface == NULL || surface->pixels == NULL || source == NULL ||
        source_width <= 0 || source_height <= 0 ||
        source_stride < source_width) {
        return false;
    }

    if (dst_x < 0) {
        source_x = -dst_x;
        copy_width -= source_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        source_y = -dst_y;
        copy_height -= source_y;
        dst_y = 0;
    }
    if (dst_x + copy_width > surface->width) copy_width = surface->width - dst_x;
    if (dst_y + copy_height > surface->height) copy_height = surface->height - dst_y;
    if (copy_width <= 0 || copy_height <= 0) return false;

    for (int y = 0; y < copy_height; ++y) {
        const pixel_t *source_row = source +
            (size_t)(source_y + y) * (size_t)source_stride + (size_t)source_x;
        pixel_t *destination_row = surface->pixels +
            (size_t)(dst_y + y) * (size_t)surface->stride + (size_t)dst_x;
        memcpy(destination_row, source_row,
               (size_t)copy_width * sizeof(pixel_t));
    }

    ui_surface_mark_dirty(surface,
                          (UiRect){dst_x, dst_y, copy_width, copy_height});
    return true;
}

void ui_surface_mark_dirty(UiSurface *surface, UiRect rect)
{
    UiRect bounds;

    if (surface == NULL) return;
    bounds = (UiRect){0, 0, surface->width, surface->height};
    rect = ui_rect_intersection(rect, bounds);
    if (ui_rect_is_empty(rect)) return;

    surface->dirty = surface->has_dirty
        ? ui_rect_union(surface->dirty, rect)
        : rect;
    surface->has_dirty = true;
}

void ui_surface_mark_all_dirty(UiSurface *surface)
{
    if (surface == NULL || surface->width <= 0 || surface->height <= 0) return;
    surface->dirty = (UiRect){0, 0, surface->width, surface->height};
    surface->has_dirty = true;
}

void ui_surface_clear_dirty(UiSurface *surface)
{
    if (surface == NULL) return;
    surface->dirty = (UiRect){0, 0, 0, 0};
    surface->has_dirty = false;
}

UiRect ui_surface_dirty_rect(const UiSurface *surface)
{
    if (surface == NULL || !surface->has_dirty) {
        return (UiRect){0, 0, 0, 0};
    }
    return surface->dirty;
}
