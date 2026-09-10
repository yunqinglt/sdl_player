#include "ui/ui_drawer.h"
#include "ui/ui_surface.h"

#include <stdio.h>
#include <stdlib.h>

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

typedef struct {
    bool called;
    UiRect root_bounds;
    UiRect control_bounds[4];
    size_t control_groups[4];
    UiRect group_bounds[4];
    size_t group_sizes[4];
    size_t control_count;
    size_t group_count;
    int grouping_threshold;
} DebugCapture;

static void capture_render_tree(const UiRenderTreeDebugSnapshot *snapshot,
                                void *context)
{
    DebugCapture *capture = context;
    size_t copy_count = snapshot->control_count;

    if (copy_count > ARRAY_COUNT(capture->control_groups)) {
        copy_count = ARRAY_COUNT(capture->control_groups);
    }
    capture->called = true;
    capture->root_bounds = snapshot->root_bounds;
    capture->control_count = snapshot->control_count;
    capture->group_count = snapshot->group_count;
    capture->grouping_threshold = snapshot->grouping_threshold;
    for (size_t i = 0; i < copy_count; ++i) {
        capture->control_bounds[i] = snapshot->control_bounds[i];
        capture->control_groups[i] = snapshot->control_groups[i];
        capture->group_bounds[i] = snapshot->group_bounds[i];
        capture->group_sizes[i] = snapshot->group_sizes[i];
    }
}

static void draw_solid(UiBuffer *buffer, void *context)
{
    const pixel_t color = *(const pixel_t *)context;

    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x < buffer->width; ++x) {
            buffer->pixels[(size_t)y * (size_t)buffer->stride + (size_t)x] =
                color;
        }
    }
}

static bool test_rects(void)
{
    UiRect intersection = ui_rect_intersection((UiRect){-2, 2, 6, 5},
                                                (UiRect){0, 0, 8, 4});
    UiRect united = ui_rect_union((UiRect){1, 2, 3, 4},
                                  (UiRect){5, 1, 2, 2});
    CHECK(intersection.x == 0 && intersection.y == 2);
    CHECK(intersection.w == 4 && intersection.h == 2);
    CHECK(united.x == 1 && united.y == 1);
    CHECK(united.w == 6 && united.h == 5);
    return true;
}

static bool test_clipped_blit_and_dirty_union(void)
{
    UiSurface surface;
    const pixel_t source[12] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12
    };

    CHECK(ui_surface_create(&surface, 5, 4));
    ui_surface_clear_dirty(&surface);
    CHECK(ui_surface_blit(&surface, source, 4, 3, 4, -2, 1));
    CHECK(surface.pixels[1 * surface.stride + 0] == 3);
    CHECK(surface.pixels[1 * surface.stride + 1] == 4);
    CHECK(surface.pixels[3 * surface.stride + 0] == 11);
    CHECK(surface.dirty.x == 0 && surface.dirty.y == 1);
    CHECK(surface.dirty.w == 2 && surface.dirty.h == 3);

    ui_surface_mark_dirty(&surface, (UiRect){4, 0, 4, 2});
    CHECK(surface.dirty.x == 0 && surface.dirty.y == 0);
    CHECK(surface.dirty.w == 5 && surface.dirty.h == 4);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_ui_tree_is_platform_free(void)
{
    UiSurface surface;
    UiBuffer *root;
    UiRoundedRectStyle style = {2, COLOR_WHITE, COLOR_BLUE, true};
    UiControl controls[] = {
        {{1, 1, 5, 4}, ui_draw_rounded_rect, &style},
        {{7, 2, 4, 4}, ui_draw_debug_border, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 10));
    ui_surface_fill(&surface, COLOR_BLACK);
    ui_surface_clear_dirty(&surface);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree(root, controls, 2, 2));
    ui_buffer_render(root);
    CHECK(surface.has_dirty);
    CHECK(surface.dirty.x == 1 && surface.dirty.y == 1);
    CHECK(surface.dirty.w == 10 && surface.dirty.h == 5);
    CHECK(surface.pixels[1 * surface.stride + 3] == COLOR_WHITE);
    CHECK(surface.pixels[3 * surface.stride + 3] == COLOR_BLUE);
    CHECK(surface.pixels[2 * surface.stride + 7] == COLOR_RED);
    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_render_tree_debug_snapshot(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{-2, 1, 4, 3}, NULL, NULL},
        {{3, 1, 3, 3}, NULL, NULL},
        {{12, 1, 2, 2}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 8));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     2, &debug));
    CHECK(capture.called);
    CHECK(capture.root_bounds.x == 0 && capture.root_bounds.y == 0);
    CHECK(capture.root_bounds.w == 16 && capture.root_bounds.h == 8);
    CHECK(capture.control_count == 3 && capture.group_count == 2);
    CHECK(capture.grouping_threshold == 2);
    CHECK(capture.control_groups[0] == 0);
    CHECK(capture.control_groups[1] == 0);
    CHECK(capture.control_groups[2] == 2);
    CHECK(capture.group_sizes[0] == 2);
    CHECK(capture.group_sizes[1] == 0);
    CHECK(capture.group_sizes[2] == 1);
    CHECK(capture.control_bounds[0].x == 0);
    CHECK(capture.control_bounds[0].w == 2);
    CHECK(capture.group_bounds[0].x == 0 && capture.group_bounds[0].y == 1);
    CHECK(capture.group_bounds[0].w == 6 && capture.group_bounds[0].h == 3);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_fully_clipped_control_has_no_group(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{-4, 1, 2, 2}, NULL, NULL},
        {{0, 1, 2, 2}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 10, 6));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     3, &debug));
    CHECK(capture.called);
    CHECK(capture.group_count == 1);
    CHECK(ui_rect_is_empty(capture.control_bounds[0]));
    CHECK(capture.control_groups[0] == UI_RENDER_TREE_NO_GROUP);
    CHECK(capture.control_groups[1] == 1);
    CHECK(capture.group_sizes[0] == 0);
    CHECK(capture.group_sizes[1] == 1);
    CHECK(capture.group_bounds[1].x == 0);
    CHECK(capture.group_bounds[1].y == 1);
    CHECK(capture.group_bounds[1].w == 2);
    CHECK(capture.group_bounds[1].h == 2);
    CHECK(root->first_child != NULL);
    CHECK(!root->first_child->is_group_container);
    CHECK(root->first_child->next_sibling == NULL);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_grouping_threshold_is_strict(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{1, 1, 3, 3}, NULL, NULL},
        {{7, 1, 3, 3}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 8));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     3, &debug));
    CHECK(capture.called);
    CHECK(capture.group_count == 2);
    CHECK(capture.control_groups[0] == 0);
    CHECK(capture.control_groups[1] == 1);
    CHECK(capture.group_sizes[0] == 1);
    CHECK(capture.group_sizes[1] == 1);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_grouping_is_transitive(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{1, 1, 2, 2}, NULL, NULL},
        {{5, 1, 2, 2}, NULL, NULL},
        {{9, 1, 2, 2}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 8));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     3, &debug));
    CHECK(capture.called);
    CHECK(capture.group_count == 1);
    CHECK(capture.control_groups[0] == 0);
    CHECK(capture.control_groups[1] == 0);
    CHECK(capture.control_groups[2] == 0);
    CHECK(capture.group_sizes[0] == 3);
    CHECK(capture.group_bounds[0].x == 1 && capture.group_bounds[0].y == 1);
    CHECK(capture.group_bounds[0].w == 10 && capture.group_bounds[0].h == 2);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_group_debug_bounds_overlay(void)
{
    UiSurface surface;
    UiBuffer *root;
    const pixel_t blue = COLOR_BLUE;
    const pixel_t green = COLOR_GREEN;
    const pixel_t white = COLOR_WHITE;
    const UiControl controls[] = {
        {{2, 2, 4, 4}, draw_solid, (void *)&blue},
        {{7, 2, 4, 4}, draw_solid, (void *)&green},
        {{15, 2, 3, 3}, draw_solid, (void *)&white}
    };

    CHECK(ui_surface_create(&surface, 20, 10));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree(root, controls, ARRAY_COUNT(controls), 2));
    CHECK(root->first_child != NULL);
    CHECK(root->first_child->is_group_container);
    CHECK(root->first_child->next_sibling != NULL);
    CHECK(!root->first_child->next_sibling->is_group_container);
    ui_buffer_render(root);
    CHECK(surface.pixels[2 * surface.stride + 3] == COLOR_BLUE);

    ui_surface_clear_dirty(&surface);
    ui_surface_mark_dirty(&surface, (UiRect){15, 2, 3, 3});
    ui_buffer_draw_group_debug_bounds(root, COLOR_RED);

    CHECK(surface.pixels[2 * surface.stride + 2] == COLOR_RED);
    CHECK(surface.pixels[2 * surface.stride + 6] == COLOR_RED);
    CHECK(surface.pixels[5 * surface.stride + 10] == COLOR_RED);
    CHECK(surface.pixels[3 * surface.stride + 3] == COLOR_BLUE);
    CHECK(surface.pixels[3 * surface.stride + 8] == COLOR_GREEN);
    CHECK(surface.pixels[2 * surface.stride + 15] == COLOR_WHITE);
    CHECK(surface.pixels[0] == COLOR_BLACK);
    CHECK(surface.dirty.x == 2 && surface.dirty.y == 2);
    CHECK(surface.dirty.w == 16 && surface.dirty.h == 4);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_ordinary_composite_is_not_group_overlay(void)
{
    UiSurface surface;
    UiBuffer *root;
    UiBuffer *composite;
    UiBuffer *left;
    UiBuffer *right;
    pixel_t blue = COLOR_BLUE;
    pixel_t green = COLOR_GREEN;

    CHECK(ui_surface_create(&surface, 12, 8));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    composite = ui_buffer_create(root, (UiRect){1, 1, 8, 4});
    CHECK(composite != NULL);
    left = ui_buffer_create(composite, (UiRect){0, 0, 3, 3});
    right = ui_buffer_create(composite, (UiRect){4, 0, 3, 3});
    CHECK(left != NULL && right != NULL);
    left->draw = draw_solid;
    left->context = &blue;
    right->draw = draw_solid;
    right->context = &green;
    ui_buffer_mark_dirty(left);
    ui_buffer_mark_dirty(right);
    ui_buffer_render(root);
    CHECK(surface.pixels[1 * surface.stride + 4] == COLOR_BLACK);

    ui_surface_clear_dirty(&surface);
    ui_buffer_draw_group_debug_bounds(root, COLOR_RED);
    CHECK(!surface.has_dirty);
    CHECK(surface.pixels[1 * surface.stride + 1] == COLOR_BLUE);
    CHECK(surface.pixels[1 * surface.stride + 4] == COLOR_BLACK);
    CHECK(surface.pixels[1 * surface.stride + 5] == COLOR_GREEN);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

int main(void)
{
    if (!test_rects() || !test_clipped_blit_and_dirty_union() ||
        !test_ui_tree_is_platform_free() ||
        !test_render_tree_debug_snapshot() ||
        !test_fully_clipped_control_has_no_group() ||
        !test_grouping_threshold_is_strict() ||
        !test_grouping_is_transitive() ||
        !test_group_debug_bounds_overlay() ||
        !test_ordinary_composite_is_not_group_overlay()) {
        return EXIT_FAILURE;
    }
    puts("ui-core-tests: all checks passed");
    return EXIT_SUCCESS;
}
