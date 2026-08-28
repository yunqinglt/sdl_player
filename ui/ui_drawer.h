/** @file ui_drawer.h Platform-independent buffered UI drawing. */

#ifndef SDL_PLAYER_UI_DRAWER_H
#define SDL_PLAYER_UI_DRAWER_H

#include "ui_surface.h"

#include <stddef.h>

typedef struct UiBuffer UiBuffer;
typedef void (*UiDrawCallback)(UiBuffer *buffer, void *context);

struct UiBuffer {
    UiSurface *surface;
    UiBuffer *parent;
    UiBuffer *first_child;
    UiBuffer *next_sibling;
    int x;
    int y;
    int width;
    int height;
    pixel_t *pixels;
    int stride;
    bool is_dirty;
    bool child_dirty;
    UiDrawCallback draw;
    void *context;
};

typedef struct {
    UiRect bounds;
    UiDrawCallback draw;
    void *context;
} UiControl;

typedef struct {
    unsigned radius;
    pixel_t border_color;
    pixel_t fill_color;
    bool fill;
} UiRoundedRectStyle;

typedef struct {
    const pixel_t *pixels;
    int width;
    int height;
    int stride;
} UiPicture;

UiBuffer *ui_buffer_root(UiSurface *surface);
UiBuffer *ui_buffer_create(UiBuffer *parent, UiRect relative_bounds);
void ui_buffer_destroy_tree(UiBuffer *buffer);
void ui_buffer_mark_dirty(UiBuffer *buffer);
void ui_buffer_render(UiBuffer *root);

/** Build a temporary grouping tree for nearby controls. */
bool ui_build_render_tree(UiBuffer *root, const UiControl *controls,
                          size_t count, int grouping_threshold);

void ui_draw_debug_border(UiBuffer *buffer, void *context);
void ui_draw_debug_group(UiBuffer *buffer, void *context);
void ui_draw_rounded_rect(UiBuffer *buffer, void *context);
void ui_draw_picture(UiBuffer *buffer, void *context);

pixel_t ui_blend_pixels(pixel_t background, pixel_t foreground, uint8_t alpha);

#endif
