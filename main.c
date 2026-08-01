/**
 * @file main.c
 * @brief sdl-player demo / test application
 *
 * Demonstrates the Display, Timer, and configuration modules by rendering
 * animated geometric patterns to the screen.
 *
 * Phases 0-3: Original built-in demo patterns (checkerboard, gradient,
 *             bouncing rect, color fill).
 * Phases 4-8: Ported ESP32 demoscene effects (plasma, tunnel, moire,
 *             fire, bouncing sprite).
 *
 * Press SPACE to cycle through phases.
 */

#include "display.h"
#include "timer.h"
#include "player_conf.h"
#include "drawer.h"

#include <SDL.h>

/* Total number of demo phases */
#define NUM_PHASES 9

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    int x, y;
    int w, h;
    int dx, dy;
} MovingBox;

extern const unsigned short A99503218B6635058DAE67B9FF007B[];

/* 运行 Fuzzy 容器重叠检测 Demo */
void run_fuzzy_demo_frame(Display *disp) {

    // get the root frame
    boxed_buffer_t *root = isolate_from_display(disp);

    static MovingBox b1 = { 100, 100, 200, 150, 4, 3 };
    static MovingBox b2 = { 400, 300, 140, 30, -3, 5 };

    // b1.x += b1.dx; b1.y += b1.dy;
    // if (b1.x <= 0 || b1.x + b1.w >= disp->width)  b1.dx = -b1.dx;
    // if (b1.y <= 0 || b1.y + b1.h >= disp->height) b1.dy = -b1.dy;

    // b2.x += b2.dx; b2.y += b2.dy;
    // if (b2.x <= 0 || b2.x + b2.w >= disp->width)  b2.dx = -b2.dx;
    // if (b2.y <= 0 || b2.y + b2.h >= disp->height) b2.dy = -b2.dy;

    b1.x += b1.dx; b1.y += b1.dy;
    if (b1.x <= 0) { b1.x = 0; b1.dx = -b1.dx; }
    else if (b1.x + b1.w >= disp->width) { b1.x = disp->width - b1.w; b1.dx = -b1.dx; }
    
    if (b1.y <= 0) { b1.y = 0; b1.dy = -b1.dy; }
    else if (b1.y + b1.h >= disp->height) { b1.y = disp->height - b1.h; b1.dy = -b1.dy; }

    b2.x += b2.dx; b2.y += b2.dy;
    if (b2.x <= 0) { b2.x = 0; b2.dx = -b2.dx; }
    else if (b2.x + b2.w >= disp->width) { b2.x = disp->width - b2.w; b2.dx = -b2.dx; }
    
    if (b2.y <= 0) { b2.y = 0; b2.dy = -b2.dy; }
    else if (b2.y + b2.h >= disp->height) { b2.y = disp->height - b2.h; b2.dy = -b2.dy; }

    rounded_rect_config_t *rct = (rounded_rect_config_t *) calloc(1, sizeof(rounded_rect_config_t));
    rct->border_color = RGB565_WHITE;
    rct->fill_color = RGB565_CYAN;
    rct->draw_fill = true;
    rct->radius = 20;


    // 2. 清理底层屏幕 (模拟 LCD Clear)
    Display_fill(disp, COLOR_BLACK);

    // 3. 构建渲染树
    
    boxed_buffer_t *out1 = NULL;
    boxed_buffer_t *out2 = NULL;
    

    int fuzzy_status = new_buffer_fuzzy(root, 
                                        b1.x, b1.y, b1.w, b1.h,
                                        b2.x, b2.y, b2.w, b2.h,
                                        40,
                                        &out1, &out2);

    // uint32_t *a = 1, *b = 2;

    // out1->ctx = a;
    // out2->ctx = b;

    if (fuzzy_status == 1) {
        // get their parent node -> fuzzy buffer
        boxed_buffer_t *fuzzy_parent = out1->super;
        
        // 挂载树结构：Root -> Fuzzy_Parent -> [out1, out2]
        link_buffer_node(root, fuzzy_parent);
        link_buffer_node(fuzzy_parent, out1);
        link_buffer_node(fuzzy_parent, out2);

        // 设置回调函数
        fuzzy_parent->draw_cb = draw_fuzzy_cb;

        // out1->draw_cb = draw_exact_cb;
        out1->draw_cb = draw_rounded_rect_cb;
        out1->ctx = rct;

        out2->draw_cb = draw_exact_cb;

        // 标记脏区触发渲染
        mark_buffer_dirty(fuzzy_parent);
        mark_buffer_dirty(out1);
        mark_buffer_dirty(out2);

    } else if (fuzzy_status == 0) {

        link_buffer_node(root, out1);
        link_buffer_node(root, out2);
        // link_buffer_node(root, out1);

        // out1->draw_cb = draw_exact_cb;
        out1->draw_cb = draw_rounded_rect_cb;
        out1->ctx = rct;

        out2->draw_cb = draw_exact_cb;

        mark_buffer_dirty(out1);
        mark_buffer_dirty(out2);
    }

    render_sequence(root);

    // out1->draw_cb(out1, out1->ctx);
    // out2->draw_cb(out2, out2->ctx);

    Display_markFullDirty(disp);
    Display_present(disp);

    free_buffer_tree(root);
}

void run_smart_container_frame(Display *disp) {
    boxed_buffer_t *root = isolate_from_display(disp);

    static MovingBox b1 = { 100, 100, 240, 320, 4, 3 };
    static MovingBox b2 = { 0, 0, 140, 60, -3, 5 };
    static MovingBox b3 = {700, 450, 120, 120, 2, -7};

    b1.x += b1.dx; b1.y += b1.dy;
    if (b1.x <= 0) { b1.x = 0; b1.dx = -b1.dx; }
    else if (b1.x + b1.w >= disp->width) { b1.x = disp->width - b1.w; b1.dx = -b1.dx; }
    
    if (b1.y <= 0) { b1.y = 0; b1.dy = -b1.dy; }
    else if (b1.y + b1.h >= disp->height) { b1.y = disp->height - b1.h; b1.dy = -b1.dy; }

    b2.x += b2.dx; b2.y += b2.dy;
    if (b2.x <= 0) { b2.x = 0; b2.dx = -b2.dx; }
    else if (b2.x + b2.w >= disp->width) { b2.x = disp->width - b2.w; b2.dx = -b2.dx; }
    
    if (b2.y <= 0) { b2.y = 0; b2.dy = -b2.dy; }
    else if (b2.y + b2.h >= disp->height) { b2.y = disp->height - b2.h; b2.dy = -b2.dy; }

    b3.x += b3.dx; b3.y += b3.dy;
    if (b3.x <= 0) { b3.x = 0; b3.dx = -b3.dx; }
    else if (b3.x + b3.w >= disp->width) { b3.x = disp->width - b3.w; b3.dx = -b3.dx; }
    
    if (b3.y <= 0) { b3.y = 0; b3.dy = -b3.dy; }
    else if (b3.y + b3.h >= disp->height) { b3.y = disp->height - b3.h; b3.dy = -b3.dy; }

    Display_fill(disp, COLOR_BLACK);

    static rounded_rect_config_t rct = { 20, RGB565_WHITE, RGB565_CYAN, true };
    static picture_config_t pic_cfg = { (const pixel_t*) A99503218B6635058DAE67B9FF007B, 240, 320 };
    // static grayscale_font_config_t font_cfg = { FontData_16x16, 16, 16, COLOR_RED };

    ui_control_t *controls = (ui_control_t *) calloc(3, sizeof(ui_control_t));
    // ui_control_t controls[3];
    


    controls[0].x = b1.x; controls[0].y = b1.y;
    controls[0].w = b1.w; controls[0].h = b1.h;
    controls[0].draw_cb = draw_picture_cb;
    controls[0].ctx = &pic_cfg;

    controls[1].x = b2.x; controls[1].y = b2.y;
    controls[1].w = b2.w; controls[1].h = b2.h;
    controls[1].draw_cb = draw_rounded_rect_cb;
    controls[1].ctx = &rct;
    
    controls[2].x = b3.x; controls[2].y = b3.y;
    controls[2].w = b3.w;  controls[2].h = b3.h;
    controls[2].draw_cb = draw_exact_cb;
    controls[2].ctx = NULL;


    // LOG_DEBUG("build(%X)\n", root);

    build_smart_render_tree(root, controls, 3, 40);

    render_sequence(root);

    Display_markFullDirty(disp);
    Display_present(disp);

    free_buffer_tree(root);
}

#if FPS_ENABLE
static struct {
    uint32_t frame_count;
    uint32_t last_print_time;
    float    current_fps;
} s_fps;
#endif

static void fps_init(void)
{
#if FPS_ENABLE
    s_fps.frame_count     = 0;
    s_fps.last_print_time = SDL_GetTicks();
    s_fps.current_fps     = 0.0f;
#endif
}


static void fps_tick(void)
{
#if FPS_ENABLE
    s_fps.frame_count++;
    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - s_fps.last_print_time;

    if (elapsed >= 1000) {
        s_fps.current_fps = (float)s_fps.frame_count * 1000.0f / (float)elapsed;
        LOG_INFO("FPS: %.1f\n", s_fps.current_fps);
        s_fps.frame_count     = 0;
        s_fps.last_print_time = now;
    }
#else
    (void)0;
#endif
}

/* ===================================================================
 *  Timer callbacks
 * =================================================================== */

static void on_timer_tick(void *user_data)
{
    (void)user_data;
    LOG_DEBUG("Timer callback fired!\n");
}

/* ===================================================================
 *  Drawing helpers (pixel-format independent)
 * =================================================================== */

#if defined(PIXEL_FORMAT_RGB565)

static pixel_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    return RGB565(r, g, b);
}

static pixel_t pixel_black(void) { return RGB565_BLACK; }
static pixel_t pixel_white(void) { return RGB565_WHITE; }

#else /* RGB888 */

static pixel_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    return RGB888(r, g, b);
}

static pixel_t pixel_black(void) { return RGB888_BLACK; }
static pixel_t pixel_white(void) { return RGB888_WHITE; }

#endif

static void fill_checkerboard(pixel_t *block, int w, int h, int cell_size, uint32_t tick)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int cx = x / cell_size;
            int cy = y / cell_size;
            int idx = y * w + x;

            int offset = (tick / 5) % (cell_size * 2);
            int shifted_cx = (x + offset) / cell_size;

            if ((cx + cy) % 2 == 0) {
                block[idx] = make_pixel(
                    (uint8_t)((cx * 50 + tick) % 256),
                    (uint8_t)((cy * 50 + tick) % 256),
                    (uint8_t)((cx * 30 + cy * 30 + tick) % 256));
            } else {
                block[idx] = (shifted_cx % 2 == 0) ? pixel_black() : pixel_white();
            }
        }
    }
}

static void fill_gradient(pixel_t *block, int w, int h, uint32_t tick)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            uint8_t r = (uint8_t)((x * 256 / w + tick / 10) % 256);
            uint8_t g = (uint8_t)((y * 256 / h + tick / 10) % 256);
            uint8_t b = (uint8_t)(((x + y) * 128 / (w + h) + tick / 10) % 256);
            block[idx] = make_pixel(r, g, b);
        }
    }
}


static void fill_rect(pixel_t *block, int w, int h,
                      int rx, int ry, int rw, int rh,
                      pixel_t color, pixel_t bg)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
                block[idx] = color;
            } else {
                block[idx] = bg;
            }
        }
    }
}


/* ===================================================================
 *  Main Entry Point
 * =================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const char *fmt_name;
#if defined(PIXEL_FORMAT_RGB565)
    fmt_name = "RGB565";
#else
    fmt_name = "RGB888";
#endif

    LOG_INFO("sdl-player v0.1.0 starting...\n");
    LOG_INFO("Screen: %dx%d, pixel format: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, fmt_name);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        LOG_ERR("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    atexit(SDL_Quit);

    Display *disp = Display_create("sdl-player: Embedded Display Demo");
    if (disp == NULL) {
        LOG_ERR("Failed to create Display\n");
        return 1;
    }

    fps_init();

    /* Initialise ESP32 effect tables */
    // Effects_init();

    Timer demo_timer;
    Timer_init(&demo_timer, 2000, on_timer_tick, NULL);
    Timer_start(&demo_timer);

    int phase = 0;
    uint32_t tick = 0;

    size_t fb_bytes = (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * PIXEL_BYTES;
    pixel_t *block = (pixel_t *)malloc(fb_bytes);
    if (block == NULL) {
        LOG_ERR("Failed to allocate block buffer\n");
        Display_destroy(disp);
        return 1;
    }

    /* Allocate sub-block for phase 2 on the heap (avoids VLA / big stack) */
    int rect_w = SCREEN_WIDTH / 4;
    int rect_h = SCREEN_HEIGHT / 4;
    int sub_w = rect_w + 4;
    int sub_h = rect_h + 4;
    size_t sub_bytes = (size_t)sub_w * sub_h * PIXEL_BYTES;
    pixel_t *sub_block = (pixel_t *)malloc(sub_bytes);
    if (sub_block == NULL) {
        LOG_ERR("Failed to allocate sub-block\n");
        free(block);
        Display_destroy(disp);
        return 1;
    }

    int rect_x = 0, rect_y = 0;
    int rect_dx = 3, rect_dy = 2;

    pixel_t current_color;
#if defined(PIXEL_FORMAT_RGB565)
    current_color = RGB565(0, 0, 0);
#else
    current_color = RGB888(0, 0, 0);
#endif

#if defined(PIXEL_FORMAT_RGB565)
    pixel_t rect_color = RGB565(255, 100, 50);
    pixel_t bg_color   = RGB565_BLACK;
#else
    pixel_t rect_color = RGB888(255, 100, 50);
    pixel_t bg_color   = RGB888_BLACK;
#endif

    LOG_INFO("Entering main loop (ESC to exit, SPACE to switch demo)...\n");

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    phase = (phase + 1) % 4;
                    LOG_INFO("Switched to demo phase %d\n", phase);
                }
            }
        }

        tick++;
        Timer_tick(&demo_timer);

        switch (phase) {
            case 0: {
                /*
                 * Phase 0: full-screen animated checkerboard.
                 * Write entire framebuffer, then present once.
                 * Even though we write full-screen, only one
                 * SDL_UpdateTexture call happens on present().
                 */
                fill_checkerboard(block, SCREEN_WIDTH, SCREEN_HEIGHT, 32, tick);
                Display_draw(disp, block, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
                Display_present(disp);
                break;
            }

            case 1: {
                /*
                 * Phase 1: full-screen animated gradient.
                 * Same pattern — write, then flush once.
                 */
                fill_gradient(block, SCREEN_WIDTH, SCREEN_HEIGHT, tick);
                Display_draw(disp, block, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
                Display_present(disp);
                break;
            }

            case 2: {
                /*
                 * Phase 2: bouncing rectangle with partial updates.
                 *
                 * Demonstrates dirty-region tracking:
                 *   - Erase old rect via direct framebuffer access +
                 *     Display_setDirtyRegion()  (manual dirty marking)
                 *   - Draw new rect via Display_draw() (auto dirty marking)
                 *   - Single Display_present() flushes MINIMAL region
                 *
                 * This mimics SPI displays where you set column/page
                 * address (CASET/PASET) for only the changed area.
                 */
                static int prev_x = 0, prev_y = 0;

                /* Erase old rectangle directly in framebuffer */
                if (tick > 1) {
                    pixel_t *fb = Display_getFramebuffer(disp);
                    for (int row = 0; row < sub_h; row++) {
                        int dest_y = prev_y + row;
                        pixel_t *dest = fb + (size_t)dest_y * SCREEN_WIDTH + prev_x;
                        for (int col = 0; col < sub_w; col++) {
                            dest[col] = bg_color;
                        }
                    }
                    /* Manually mark the erased area dirty */
                    Display_setDirtyRegion(disp, prev_x, prev_y, sub_w, sub_h);
                }

                /* Update position */
                rect_x += rect_dx;
                rect_y += rect_dy;
                if (rect_x <= 0 || rect_x + rect_w >= SCREEN_WIDTH)  rect_dx = -rect_dx;
                if (rect_y <= 0 || rect_y + rect_h >= SCREEN_HEIGHT) rect_dy = -rect_dy;

                /* Draw rectangle at new position (auto-marks dirty) */
                fill_rect(sub_block, sub_w, sub_h,
                          2, 2, rect_w, rect_h, rect_color, bg_color);
                Display_draw(disp, sub_block, rect_x, rect_y,
                             rect_x + sub_w - 1, rect_y + sub_h - 1);

                /* Save for next frame */
                prev_x = rect_x;
                prev_y = rect_y;

                /* Single flush for both dirty regions (they get merged) */
                Display_present(disp);
                break;
            }

            case 3: {
                // run_fuzzy_demo_frame(disp);
                run_smart_container_frame(disp);

                break;
            }

            default:
                break;
        }

        fps_tick();

#if TARGET_FPS > 0
        {
            static uint32_t last_frame_time = 0;
            uint32_t now = SDL_GetTicks();
            uint32_t frame_time = now - last_frame_time;
            uint32_t target_ms = 1000 / TARGET_FPS;
            if (frame_time < target_ms) {
                Timer_delay(target_ms - frame_time);
            }
            last_frame_time = SDL_GetTicks();
        }
#endif
    }

    free(sub_block);
    free(block);
    Display_destroy(disp);

    LOG_INFO("sdl-player exiting. Goodbye!\n");

    return 0;
}
