#include "ui/ui_drawer.h"
#include "ui/ui_surface.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

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

int main(void)
{
    if (!test_rects() || !test_clipped_blit_and_dirty_union() ||
        !test_ui_tree_is_platform_free()) {
        return EXIT_FAILURE;
    }
    puts("ui-core-tests: all checks passed");
    return EXIT_SUCCESS;
}
