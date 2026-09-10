/** @file ui_drawer.h Platform-independent buffered UI drawing. */

#ifndef SDL_PLAYER_UI_DRAWER_H
#define SDL_PLAYER_UI_DRAWER_H

#include "ui_surface.h"

#include <stddef.h>
#include <stdint.h>

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
    bool is_group_container;
};

typedef struct {
    UiRect bounds;
    UiDrawCallback draw;
    void *context;
} UiControl;

#define UI_RENDER_TREE_NO_GROUP SIZE_MAX

/**
 * Ephemeral description of a successfully built render tree.
 *
 * control_bounds and control_groups contain control_count entries. Bounds are
 * clipped to root_bounds. A fully clipped control is reported as
 * UI_RENDER_TREE_NO_GROUP and is not counted as a visible member. Raw
 * proximity components are computed before clipping. Group arrays are indexed
 * by stable group ID: the smallest visible original control index in that raw
 * component. Consequently, valid group IDs can be sparse; iterate up to
 * control_count and skip entries whose group_sizes value is 0. group_count is
 * the number of non-empty visible groups, including isolated controls. All
 * array pointers are valid only for the duration of the callback.
 */
typedef struct {
    UiRect root_bounds;
    const UiRect *control_bounds;
    const size_t *control_groups;
    const UiRect *group_bounds;
    const size_t *group_sizes;
    size_t control_count;
    size_t group_count;
    int grouping_threshold;
} UiRenderTreeDebugSnapshot;

typedef void (*UiRenderTreeDebugCallback)(
    const UiRenderTreeDebugSnapshot *snapshot, void *context);

typedef struct {
    UiRenderTreeDebugCallback callback;
    void *context;
} UiRenderTreeDebugOptions;

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

/** Build a grouping tree and optionally observe its completed topology. */
bool ui_build_render_tree_debug(UiBuffer *root, const UiControl *controls,
                                size_t count, int grouping_threshold,
                                const UiRenderTreeDebugOptions *debug);

/** Draw post-render outlines around multi-control grouping containers. */
void ui_buffer_draw_group_debug_bounds(UiBuffer *root, pixel_t color);

void ui_draw_debug_border(UiBuffer *buffer, void *context);
void ui_draw_debug_group(UiBuffer *buffer, void *context);
void ui_draw_rounded_rect(UiBuffer *buffer, void *context);
void ui_draw_picture(UiBuffer *buffer, void *context);

pixel_t ui_blend_pixels(pixel_t background, pixel_t foreground, uint8_t alpha);

#endif
