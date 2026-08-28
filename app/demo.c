#include "demo.h"

#include "esp32_effects.h"
#include "ui/ui_drawer.h"

#include <stddef.h>

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int dx;
    int dy;
} MovingBox;

static void move_box(MovingBox *box, int width, int height)
{
    box->x += box->dx;
    box->y += box->dy;

    if (box->x < 0) {
        box->x = 0;
        box->dx = -box->dx;
    } else if (box->x + box->width > width) {
        box->x = width - box->width;
        box->dx = -box->dx;
    }
    if (box->y < 0) {
        box->y = 0;
        box->dy = -box->dy;
    } else if (box->y + box->height > height) {
        box->y = height - box->height;
        box->dy = -box->dy;
    }
}

static void render_checkerboard(Demo *demo)
{
    UiSurface *surface = demo->surface;
    const int cell = 32;
    const int offset = (int)(demo->tick / 5u) % (cell * 2);

    for (int y = 0; y < surface->height; ++y) {
        for (int x = 0; x < surface->width; ++x) {
            const int cx = x / cell;
            const int cy = y / cell;
            pixel_t color;

            if ((cx + cy) % 2 == 0) {
                color = PIXEL_RGB((cx * 50 + demo->tick) & 0xffu,
                                  (cy * 50 + demo->tick) & 0xffu,
                                  (cx * 30 + cy * 30 + demo->tick) & 0xffu);
            } else {
                color = ((x + offset) / cell) % 2 == 0
                    ? COLOR_BLACK : COLOR_WHITE;
            }
            surface->pixels[(size_t)y * (size_t)surface->stride + (size_t)x] = color;
        }
    }
    ui_surface_mark_all_dirty(surface);
}

static void render_gradient(Demo *demo)
{
    UiSurface *surface = demo->surface;

    for (int y = 0; y < surface->height; ++y) {
        for (int x = 0; x < surface->width; ++x) {
            const unsigned r = ((unsigned)x * 256u / (unsigned)surface->width +
                                demo->tick / 10u) & 0xffu;
            const unsigned g = ((unsigned)y * 256u / (unsigned)surface->height +
                                demo->tick / 10u) & 0xffu;
            const unsigned b = ((unsigned)(x + y) * 128u /
                                (unsigned)(surface->width + surface->height) +
                                demo->tick / 10u) & 0xffu;
            surface->pixels[(size_t)y * (size_t)surface->stride + (size_t)x] =
                PIXEL_RGB(r, g, b);
        }
    }
    ui_surface_mark_all_dirty(surface);
}

static void fill_surface_rect(UiSurface *surface, UiRect rect, pixel_t color)
{
    rect = ui_rect_intersection(rect,
                                (UiRect){0, 0, surface->width, surface->height});
    if (ui_rect_is_empty(rect)) return;

    for (int y = rect.y; y < rect.y + rect.h; ++y) {
        for (int x = rect.x; x < rect.x + rect.w; ++x) {
            surface->pixels[(size_t)y * (size_t)surface->stride + (size_t)x] = color;
        }
    }
    ui_surface_mark_dirty(surface, rect);
}

static void render_partial_bounce(Demo *demo)
{
    UiSurface *surface = demo->surface;
    const int width = surface->width / 4;
    const int height = surface->height / 4;
    const UiRect previous = {demo->rect_x, demo->rect_y, width, height};

    fill_surface_rect(surface, previous, COLOR_BLACK);
    demo->rect_x += demo->rect_dx;
    demo->rect_y += demo->rect_dy;

    if (demo->rect_x < 0) {
        demo->rect_x = 0;
        demo->rect_dx = -demo->rect_dx;
    } else if (demo->rect_x + width > surface->width) {
        demo->rect_x = surface->width - width;
        demo->rect_dx = -demo->rect_dx;
    }
    if (demo->rect_y < 0) {
        demo->rect_y = 0;
        demo->rect_dy = -demo->rect_dy;
    } else if (demo->rect_y + height > surface->height) {
        demo->rect_y = surface->height - height;
        demo->rect_dy = -demo->rect_dy;
    }

    fill_surface_rect(surface,
                      (UiRect){demo->rect_x, demo->rect_y, width, height},
                      PIXEL_RGB(255, 100, 50));
    fill_surface_rect(surface,
                      (UiRect){demo->rect_x + 2, demo->rect_y + 2,
                               width - 4, height - 4},
                      PIXEL_RGB(30, 70, 180));
}

static bool render_ui_tree(Demo *demo)
{
    UiSurface *surface = demo->surface;
    UiBuffer *root;
    static MovingBox boxes[] = {
        {80, 80, 240, 180, 4, 3},
        {410, 280, 180, 90, -3, 5},
        {650, 430, 120, 120, 2, -7}
    };
    static const UiRoundedRectStyle styles[] = {
        {24, COLOR_WHITE, PIXEL_RGB(24, 110, 180), true},
        {16, COLOR_YELLOW, PIXEL_RGB(100, 35, 125), true},
        {30, COLOR_CYAN, PIXEL_RGB(10, 70, 35), true}
    };
    UiControl controls[sizeof(boxes) / sizeof(boxes[0])];

    ui_surface_fill(surface, PIXEL_RGB(8, 12, 18));
    for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i) {
        move_box(&boxes[i], surface->width, surface->height);
        controls[i].bounds = (UiRect){boxes[i].x, boxes[i].y,
                                      boxes[i].width, boxes[i].height};
        controls[i].draw = ui_draw_rounded_rect;
        controls[i].context = (void *)&styles[i];
    }

    root = ui_buffer_root(surface);
    if (root == NULL) return false;
    if (!ui_build_render_tree(root, controls,
                              sizeof(controls) / sizeof(controls[0]), 40)) {
        ui_buffer_destroy_tree(root);
        return false;
    }
    ui_buffer_render(root);
    ui_buffer_destroy_tree(root);
    return true;
}

bool demo_init(Demo *demo, UiSurface *surface)
{
    if (demo == NULL || surface == NULL || surface->pixels == NULL) return false;
    *demo = (Demo){0};
    demo->surface = surface;
    demo->rect_dx = 3;
    demo->rect_dy = 2;
    Effects_init();
    ui_surface_fill(surface, COLOR_BLACK);
    return true;
}

void demo_set_phase(Demo *demo, unsigned phase)
{
    if (demo == NULL) return;
    demo->phase = phase % DEMO_PHASE_COUNT;
    demo->tick = 0;
    demo->rect_x = 0;
    demo->rect_y = 0;
    demo->rect_dx = 3;
    demo->rect_dy = 2;
    if (demo->phase >= 4) {
        Effects_enter((EffectMode)(demo->phase - 4));
    }
    ui_surface_fill(demo->surface, COLOR_BLACK);
}

void demo_next_phase(Demo *demo)
{
    if (demo != NULL) demo_set_phase(demo, demo->phase + 1u);
}

const char *demo_phase_name(const Demo *demo)
{
    static const char *const names[DEMO_PHASE_COUNT] = {
        "checkerboard", "gradient", "partial update", "platform-free UI tree",
        "plasma", "tunnel", "moire", "fire", "bouncing sprite"
    };
    return demo == NULL ? "invalid" : names[demo->phase % DEMO_PHASE_COUNT];
}

bool demo_render(Demo *demo)
{
    if (demo == NULL || demo->surface == NULL) return false;
    ++demo->tick;

    switch (demo->phase) {
    case 0: render_checkerboard(demo); return true;
    case 1: render_gradient(demo); return true;
    case 2: render_partial_bounce(demo); return true;
    case 3: return render_ui_tree(demo);
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        Effects_render(demo->surface->pixels,
                       (EffectMode)(demo->phase - 4), demo->tick);
        ui_surface_mark_all_dirty(demo->surface);
        return true;
    default: return false;
    }
}
