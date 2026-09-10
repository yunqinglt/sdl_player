#include "ui_drawer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int min_int(int a, int b) { return a < b ? a : b; }
static int max_int(int a, int b) { return a > b ? a : b; }

static void append_child(UiBuffer *parent, UiBuffer *child)
{
    UiBuffer **slot = &parent->first_child;
    while (*slot != NULL) slot = &(*slot)->next_sibling;
    *slot = child;
}

static UiRect absolute_bounds(const UiBuffer *buffer)
{
    UiRect result = {buffer->x, buffer->y, buffer->width, buffer->height};
    const UiBuffer *parent = buffer->parent;

    while (parent != NULL) {
        result.x += parent->x;
        result.y += parent->y;
        parent = parent->parent;
    }
    return result;
}

UiBuffer *ui_buffer_root(UiSurface *surface)
{
    UiBuffer *root;

    if (surface == NULL || surface->pixels == NULL) return NULL;
    root = calloc(1, sizeof(*root));
    if (root == NULL) return NULL;

    root->surface = surface;
    root->width = surface->width;
    root->height = surface->height;
    root->pixels = surface->pixels;
    root->stride = surface->stride;
    return root;
}

UiBuffer *ui_buffer_create(UiBuffer *parent, UiRect bounds)
{
    UiBuffer *buffer;

    if (parent == NULL || bounds.x < 0 || bounds.y < 0 ||
        bounds.w <= 0 || bounds.h <= 0 ||
        bounds.x > parent->width - bounds.w ||
        bounds.y > parent->height - bounds.h) {
        return NULL;
    }

    buffer = calloc(1, sizeof(*buffer));
    if (buffer == NULL) return NULL;

    buffer->surface = parent->surface;
    buffer->parent = parent;
    buffer->x = bounds.x;
    buffer->y = bounds.y;
    buffer->width = bounds.w;
    buffer->height = bounds.h;
    buffer->stride = parent->stride;
    buffer->pixels = parent->pixels +
        (size_t)bounds.y * (size_t)parent->stride + (size_t)bounds.x;
    append_child(parent, buffer);
    return buffer;
}

void ui_buffer_destroy_tree(UiBuffer *buffer)
{
    UiBuffer *child;

    if (buffer == NULL) return;
    child = buffer->first_child;
    while (child != NULL) {
        UiBuffer *next = child->next_sibling;
        ui_buffer_destroy_tree(child);
        child = next;
    }
    free(buffer);
}

void ui_buffer_mark_dirty(UiBuffer *buffer)
{
    UiBuffer *ancestor;

    if (buffer == NULL) return;
    buffer->is_dirty = true;
    ancestor = buffer->parent;
    while (ancestor != NULL) {
        ancestor->child_dirty = true;
        ancestor = ancestor->parent;
    }
}

static void render_node(UiBuffer *buffer)
{
    if (buffer == NULL || (!buffer->is_dirty && !buffer->child_dirty)) return;

    if (buffer->is_dirty) {
        if (buffer->draw != NULL) buffer->draw(buffer, buffer->context);
        ui_surface_mark_dirty(buffer->surface, absolute_bounds(buffer));
        buffer->is_dirty = false;
    }

    for (UiBuffer *child = buffer->first_child;
         child != NULL;
         child = child->next_sibling) {
        render_node(child);
    }
    buffer->child_dirty = false;
}

void ui_buffer_render(UiBuffer *root)
{
    render_node(root);
}

static size_t set_find(size_t *parents, size_t item)
{
    size_t root = item;
    while (parents[root] != root) root = parents[root];
    while (parents[item] != item) {
        size_t next = parents[item];
        parents[item] = root;
        item = next;
    }
    return root;
}

static bool controls_are_near(UiRect a, UiRect b, int threshold)
{
    return !(a.x + a.w + threshold <= b.x ||
             b.x + b.w + threshold <= a.x ||
             a.y + a.h + threshold <= b.y ||
             b.y + b.h + threshold <= a.y);
}

bool ui_build_render_tree_debug(UiBuffer *root, const UiControl *controls,
                                size_t count, int threshold,
                                const UiRenderTreeDebugOptions *debug)
{
    size_t *parents = NULL;
    size_t *group_sizes = NULL;
    UiRect *group_bounds = NULL;
    UiRect *clipped_bounds = NULL;
    size_t group_count = 0;
    bool success = false;
    const UiRect root_bounds = {0, 0,
                                root != NULL ? root->width : 0,
                                root != NULL ? root->height : 0};

    if (root == NULL || controls == NULL || count == 0) return false;
    if (threshold < 0) threshold = 0;

    parents = malloc(count * sizeof(*parents));
    group_sizes = malloc(count * sizeof(*group_sizes));
    group_bounds = calloc(count, sizeof(*group_bounds));
    clipped_bounds = malloc(count * sizeof(*clipped_bounds));
    if (parents == NULL || group_sizes == NULL || group_bounds == NULL ||
        clipped_bounds == NULL) {
        goto done;
    }

    for (size_t i = 0; i < count; ++i) parents[i] = i;
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (controls_are_near(controls[i].bounds,
                                  controls[j].bounds, threshold)) {
                size_t root_i = set_find(parents, i);
                size_t root_j = set_find(parents, j);
                if (root_i != root_j) parents[root_i] = root_j;
            }
        }
    }

    /*
     * Keep the raw proximity components, but expose only controls which
     * actually render. A component's stable ID is its smallest visible source
     * index; clipped controls have no group even when they are near a visible
     * member.
     */
    for (size_t i = 0; i < count; ++i) {
        parents[i] = set_find(parents, i);
        clipped_bounds[i] =
            ui_rect_intersection(controls[i].bounds, root_bounds);
    }
    for (size_t i = 0; i < count; ++i) {
        group_sizes[i] = UI_RENDER_TREE_NO_GROUP;
    }
    for (size_t i = 0; i < count; ++i) {
        size_t root_id;

        if (ui_rect_is_empty(clipped_bounds[i])) continue;
        root_id = parents[i];
        if (i < group_sizes[root_id]) group_sizes[root_id] = i;
    }
    for (size_t i = 0; i < count; ++i) {
        parents[i] = ui_rect_is_empty(clipped_bounds[i])
            ? UI_RENDER_TREE_NO_GROUP
            : group_sizes[parents[i]];
    }
    memset(group_sizes, 0, count * sizeof(*group_sizes));

    for (size_t i = 0; i < count; ++i) {
        size_t group = parents[i];

        if (group == UI_RENDER_TREE_NO_GROUP) continue;
        group_bounds[group] = group_sizes[group] == 0
            ? clipped_bounds[i]
            : ui_rect_union(group_bounds[group], clipped_bounds[i]);
        ++group_sizes[group];
    }

    for (size_t group = 0; group < count; ++group) {
        if (group_sizes[group] != 0) ++group_count;
    }

    for (size_t group = 0; group < count; ++group) {
        UiBuffer *container = root;
        UiRect bounds = group_bounds[group];

        if (group_sizes[group] == 0) continue;
        if (group_sizes[group] > 1) {
            container = ui_buffer_create(root, bounds);
            if (container == NULL) goto done;
            container->is_group_container = true;
        }

        for (size_t i = 0; i < count; ++i) {
            UiBuffer *child;
            UiRect relative;

            if (parents[i] != group) continue;
            if (ui_rect_is_empty(clipped_bounds[i])) continue;
            relative = clipped_bounds[i];
            if (container != root) {
                relative.x -= bounds.x;
                relative.y -= bounds.y;
            }
            child = ui_buffer_create(container, relative);
            if (child == NULL) goto done;
            child->draw = controls[i].draw;
            child->context = controls[i].context;
            ui_buffer_mark_dirty(child);
        }
    }

    success = true;
    if (debug != NULL && debug->callback != NULL) {
        const UiRenderTreeDebugSnapshot snapshot = {
            root_bounds,
            clipped_bounds,
            parents,
            group_bounds,
            group_sizes,
            count,
            group_count,
            threshold
        };
        debug->callback(&snapshot, debug->context);
    }

done:
    free(clipped_bounds);
    free(group_bounds);
    free(group_sizes);
    free(parents);
    return success;
}

bool ui_build_render_tree(UiBuffer *root, const UiControl *controls,
                          size_t count, int threshold)
{
    return ui_build_render_tree_debug(root, controls, count, threshold, NULL);
}

void ui_draw_debug_border(UiBuffer *buffer, void *context)
{
    pixel_t color = context != NULL ? *(const pixel_t *)context : COLOR_RED;

    if (buffer == NULL || buffer->width <= 0 || buffer->height <= 0) return;
    for (int x = 0; x < buffer->width; ++x) {
        buffer->pixels[x] = color;
        buffer->pixels[(size_t)(buffer->height - 1) * (size_t)buffer->stride +
                       (size_t)x] = color;
    }
    for (int y = 0; y < buffer->height; ++y) {
        buffer->pixels[(size_t)y * (size_t)buffer->stride] = color;
        buffer->pixels[(size_t)y * (size_t)buffer->stride +
                       (size_t)(buffer->width - 1)] = color;
    }
}

static void draw_group_debug_bounds(UiBuffer *buffer, pixel_t color)
{
    UiBuffer *child;

    if (buffer == NULL) return;
    for (child = buffer->first_child;
         child != NULL;
         child = child->next_sibling) {
        draw_group_debug_bounds(child, color);
    }

    if (!buffer->is_group_container || buffer->parent == NULL ||
        buffer->first_child == NULL ||
        buffer->first_child->next_sibling == NULL) {
        return;
    }
    ui_draw_debug_border(buffer, &color);
    ui_surface_mark_dirty(buffer->surface, absolute_bounds(buffer));
}

void ui_buffer_draw_group_debug_bounds(UiBuffer *root, pixel_t color)
{
    draw_group_debug_bounds(root, color);
}

void ui_draw_debug_group(UiBuffer *buffer, void *context)
{
    (void)context;
    if (buffer == NULL) return;
    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x < buffer->width; ++x) {
            const bool edge = x == 0 || y == 0 ||
                              x == buffer->width - 1 || y == buffer->height - 1;
            buffer->pixels[(size_t)y * (size_t)buffer->stride + (size_t)x] =
                edge ? COLOR_GREEN : COLOR_DARK_GREEN;
        }
    }
}

static bool rounded_contains(int x, int y, int width, int height, int radius)
{
    int dx = 0;
    int dy = 0;

    if (radius <= 0) return true;
    if (x < radius) dx = radius - 1 - x;
    else if (x >= width - radius) dx = x - (width - radius);
    if (y < radius) dy = radius - 1 - y;
    else if (y >= height - radius) dy = y - (height - radius);
    return dx * dx + dy * dy <= (radius - 1) * (radius - 1);
}

void ui_draw_rounded_rect(UiBuffer *buffer, void *context)
{
    UiRoundedRectStyle fallback = {8, COLOR_RED, COLOR_BLACK, true};
    const UiRoundedRectStyle *style = context != NULL ? context : &fallback;
    int radius;

    if (buffer == NULL || buffer->width <= 0 || buffer->height <= 0) return;
    radius = min_int((int)style->radius,
                     min_int(buffer->width, buffer->height) / 2);

    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x < buffer->width; ++x) {
            bool outer = rounded_contains(x, y, buffer->width,
                                          buffer->height, radius);
            bool inner = buffer->width > 2 && buffer->height > 2 &&
                         x > 0 && y > 0 &&
                         x < buffer->width - 1 && y < buffer->height - 1 &&
                         rounded_contains(x - 1, y - 1,
                                          buffer->width - 2,
                                          buffer->height - 2,
                                          max_int(radius - 1, 0));
            pixel_t *pixel = buffer->pixels +
                (size_t)y * (size_t)buffer->stride + (size_t)x;

            if (!outer) continue;
            if (!inner) *pixel = style->border_color;
            else if (style->fill) *pixel = style->fill_color;
        }
    }
}

void ui_draw_picture(UiBuffer *buffer, void *context)
{
    const UiPicture *picture = context;
    int copy_width;
    int copy_height;

    if (buffer == NULL || picture == NULL || picture->pixels == NULL ||
        picture->width <= 0 || picture->height <= 0 ||
        picture->stride < picture->width) {
        return;
    }

    copy_width = min_int(buffer->width, picture->width);
    copy_height = min_int(buffer->height, picture->height);
    for (int y = 0; y < copy_height; ++y) {
        for (int x = 0; x < copy_width; ++x) {
            buffer->pixels[(size_t)y * (size_t)buffer->stride + (size_t)x] =
                picture->pixels[(size_t)y * (size_t)picture->stride + (size_t)x];
        }
    }
}

pixel_t ui_blend_pixels(pixel_t background, pixel_t foreground, uint8_t alpha)
{
    if (alpha == 0) return background;
    if (alpha == 255) return foreground;

#if defined(PIXEL_FORMAT_RGB565)
    const uint32_t bg_r = (background >> 11) & 0x1fu;
    const uint32_t bg_g = (background >> 5) & 0x3fu;
    const uint32_t bg_b = background & 0x1fu;
    const uint32_t fg_r = (foreground >> 11) & 0x1fu;
    const uint32_t fg_g = (foreground >> 5) & 0x3fu;
    const uint32_t fg_b = foreground & 0x1fu;
    const uint32_t out_r = (fg_r * alpha + bg_r * (255u - alpha)) / 255u;
    const uint32_t out_g = (fg_g * alpha + bg_g * (255u - alpha)) / 255u;
    const uint32_t out_b = (fg_b * alpha + bg_b * (255u - alpha)) / 255u;
    return (pixel_t)((out_r << 11) | (out_g << 5) | out_b);
#else
    const uint32_t bg_r = (background >> 16) & 0xffu;
    const uint32_t bg_g = (background >> 8) & 0xffu;
    const uint32_t bg_b = background & 0xffu;
    const uint32_t fg_r = (foreground >> 16) & 0xffu;
    const uint32_t fg_g = (foreground >> 8) & 0xffu;
    const uint32_t fg_b = foreground & 0xffu;
    const uint32_t out_r = (fg_r * alpha + bg_r * (255u - alpha)) / 255u;
    const uint32_t out_g = (fg_g * alpha + bg_g * (255u - alpha)) / 255u;
    const uint32_t out_b = (fg_b * alpha + bg_b * (255u - alpha)) / 255u;
    return (pixel_t)((out_r << 16) | (out_g << 8) | out_b);
#endif
}
