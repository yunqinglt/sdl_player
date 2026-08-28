/**
 * @file esp32_effects.c
 * @brief Ported ESP32 demo effects for sdl-player
 *
 * Ported from esp32_demo.c — the original used direct LCD hardware access
 * (LCD_FillFast, LCD_ShowPictureFast) and an LCD_W * LCD_H * 2 raw RGB565
 * buffer (rgb_buf). This version renders into a generic pixel_t buffer
 * that can be fed to Display_draw().
 *
 * Key changes from the original:
 *   - Uses pixel_t (uint16_t for RGB565, uint32_t for RGB888) instead of
 *     raw rgb_buf[] and LCD_ calls.
 *   - Replaced LCD_W/LCD_H with SCREEN_WIDTH/SCREEN_HEIGHT.
 *   - Receives an application tick; it has no hardware or SDL timer dependency.
 *   - Replaced LCD_ShowPictureFast(0,0,LCD_W,LCD_H,rgb_buf) with
 *     Display_draw(disp, block, 0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1).
 *   - Removed draw_fps() — the main.c FPS system handles that.
 *   - Bounce effect uses a simple coloured rect with border, since
 *     gImage_1 from the original isn't available.
 */

#include "esp32_effects.h"
#include "player_conf.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================
 *  Look-up tables (pre-computed for speed)
 * =================================================================== */

/** Sine LUT: 256 entries, sintab[i] = (sin(i*2pi/256) + 1) * 127.5 */
static uint8_t s_sintab[256];
static bool    s_tables_initialised = false;

/** Rainbow palette (256 entries, as pixel_t) */
static pixel_t s_palette[256];

/** Tunnel distance and angle maps (SCREEN_WIDTH * SCREEN_HEIGHT each) */
static uint8_t *s_tunnel_dist = NULL;
static uint8_t *s_tunnel_ang  = NULL;

/** Fire buffer */
static uint8_t *s_fire_buf = NULL;

/** Bounce state */
static int s_bounce_x = 0, s_bounce_y = 0;
static int s_bounce_vx = 4, s_bounce_vy = 3;

/* ===================================================================
 *  Pixel helpers (colour packing)
 * =================================================================== */

#if defined(PIXEL_FORMAT_RGB565)

static inline pixel_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    return RGB565(r, g, b);
}

#else /* RGB888 */

static inline pixel_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    return RGB888(r, g, b);
}

#endif

/* ===================================================================
 *  Initialisation
 * =================================================================== */

void Effects_init(void)
{
    if (s_tables_initialised) return;

    /* --- Sine LUT --- */
    for (int i = 0; i < 256; i++) {
        float s = sinf(i * 6.283185f / 256.0f);
        s_sintab[i] = (uint8_t)((s + 1.0f) * 127.5f);
    }

    /* --- Rainbow palette: black → purple → blue → cyan → green → yellow → red → white --- */
    for (int i = 0; i < 256; i++) {
        uint8_t r, g, b;
        if      (i < 32)  { r = i*4;          g = 0;               b = 128 - i*4; }
        else if (i < 64)  { r = 128+(i-32)*4; g = 0;               b = 0; }
        else if (i < 96)  { r = 255;          g = (i-64)*4;        b = 0; }
        else if (i < 128) { r = 255;          g = 128+(i-96)*4;    b = 0; }
        else if (i < 160) { r = 255-(i-128)*4; g = 255;            b = (i-128)*4; }
        else if (i < 192) { r = 128-(i-160)*4; g = 255;            b = 255; }
        else if (i < 224) { r = 0;            g = 255-(i-192)*4;   b = 255; }
        else              { r = (i-224)*8;    g = (i-224)*8;       b = 255; }
        s_palette[i] = make_pixel(r, g, b);
    }

    /* --- Tunnel maps --- */
    s_tunnel_dist = (uint8_t *)malloc((size_t)SCREEN_WIDTH * SCREEN_HEIGHT);
    s_tunnel_ang  = (uint8_t *)malloc((size_t)SCREEN_WIDTH * SCREEN_HEIGHT);

    if (s_tunnel_dist && s_tunnel_ang) {
        float cx = SCREEN_WIDTH  / 2.0f;
        float cy = SCREEN_HEIGHT / 2.0f;
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float d = sqrtf(dx*dx + dy*dy) * 3.5f;
                float a = atan2f(dy, dx);
                s_tunnel_dist[y * SCREEN_WIDTH + x] = (uint8_t)((int)d & 0xFF);
                s_tunnel_ang[y * SCREEN_WIDTH + x]  = (uint8_t)((a + 3.14159f) * 40.7f);
            }
        }
    }

    /* --- Fire buffer --- */
    s_fire_buf = (uint8_t *)malloc((size_t)SCREEN_WIDTH * SCREEN_HEIGHT);

    srand(12345);

    s_tables_initialised = true;
}

void Effects_enter(EffectMode mode)
{
    if (mode == EFFECT_FIRE && s_fire_buf) {
        memset(s_fire_buf, 0, (size_t)SCREEN_WIDTH * SCREEN_HEIGHT);
    }
    if (mode == EFFECT_BOUNCE) {
        s_bounce_x = 0;
        s_bounce_y = 0;
        s_bounce_vx = 4;
        s_bounce_vy = 3;
    }
}

/* ===================================================================
 *  Effect: Plasma
 * =================================================================== */

void Effects_render_plasma(pixel_t *block, uint32_t tick)
{
    uint8_t t  = (uint8_t)(tick & 0xFF);
    uint8_t tt1 = t;
    uint8_t tt2 = t * 3;
    uint8_t tt3 = t * 5;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        int base = y * SCREEN_WIDTH;
        uint8_t yv = (uint8_t)y;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t v = s_sintab[(x + tt1) & 0xFF]
                      + s_sintab[(yv + tt2) & 0xFF]
                      + s_sintab[(((x + yv) >> 1) + tt3) & 0xFF];
            block[base + x] = s_palette[v];
        }
    }
}

/* ===================================================================
 *  Effect: 3D Tunnel
 * =================================================================== */

void Effects_render_tunnel(pixel_t *block, uint32_t tick)
{
    if (!s_tunnel_dist || !s_tunnel_ang) return;

    uint8_t tt1 = (uint8_t)(tick & 0xFF);
    uint8_t tt2 = (uint8_t)((tick * 2) & 0xFF);

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint8_t v = s_sintab[(s_tunnel_dist[i] + tt1) & 0xFF]
                  + s_sintab[(s_tunnel_ang[i]  + tt2) & 0xFF];
        block[i] = s_palette[v];
    }
}

/* ===================================================================
 *  Effect: Concentric Moire Rings
 * =================================================================== */

void Effects_render_moire(pixel_t *block, uint32_t tick)
{
    float cx = SCREEN_WIDTH  / 2.0f;
    float cy = SCREEN_HEIGHT / 2.0f;
    uint8_t t = (uint8_t)(tick & 0xFF);

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        int base = y * SCREEN_WIDTH;
        float dy = y - cy;
        float dy2 = dy * dy;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            float dx = x - cx;
            int d = (int)(dx*dx + dy2) >> 4;
            uint8_t v = (uint8_t)(d + (t << 3));
            block[base + x] = s_palette[v ^ (uint8_t)(t * 11)];
        }
    }
}

/* ===================================================================
 *  Effect: Rising Fire
 * =================================================================== */

void Effects_render_fire(pixel_t *block, uint32_t tick)
{
    (void)tick;
    if (!s_fire_buf) return;

    int stride = SCREEN_WIDTH;

    /* --- Generate random heat source at bottom row --- */
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        s_fire_buf[(SCREEN_HEIGHT - 1) * stride + x] =
            (rand() > RAND_MAX / 3) ? (uint8_t)(rand() & 0xFF) : 0;
    }

    /* --- Propagate fire upward with cooling --- */
    for (int y = 0; y < SCREEN_HEIGHT - 2; y++) {
        for (int x = 1; x < SCREEN_WIDTH - 1; x++) {
            int idx = y * stride + x;
            int below = (y + 1) * stride + x;
            uint8_t v = (uint8_t)(
                (s_fire_buf[below - 1] +
                 s_fire_buf[below] +
                 s_fire_buf[below + 1] +
                 s_fire_buf[below + stride]) >> 2);
            s_fire_buf[idx] = (v > 1) ? (v - 1) : 0;
        }
    }

    /* --- Map fire buffer to RGB using fire palette --- */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint8_t v = s_fire_buf[i];
        uint8_t r, g, b;
        if (v < 32)       { r = v * 4;          g = 0;               b = 0; }
        else if (v < 64)  { r = 128 + (v-32)*4; g = 0;               b = 0; }
        else if (v < 96)  { r = 255;            g = (v-64)*4;        b = 0; }
        else if (v < 128) { r = 255;            g = 128+(v-96)*4;    b = 0; }
        else              { r = 255;            g = 255;             b = (v-128)*2; }
        block[i] = make_pixel(r, g, b);
    }
}

/* ===================================================================
 *  Effect: Bouncing Rect (replaces original pic bounce)
 * ===================================================================
 *
 * The original ESP32 demo used LCD_ShowPictureFast() with a hardcoded
 * gImage_1 bitmap. Since we don't have that bitmap, we render a colourful
 * animated "sprite" — a bouncing rectangle with gradient fill and border,
 * giving a similar visual effect.
 */

void Effects_render_bounce(pixel_t *block, uint32_t tick)
{
    (void)tick;

    /* Sprite dimensions */
    int sw = 60;
    int sh = 60;

    /* Background colour */
    pixel_t bg;
#if defined(PIXEL_FORMAT_RGB565)
    bg = RGB565_BLACK;
#else
    bg = RGB888_BLACK;
#endif

    /* Fill entire block with background first */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        block[i] = bg;
    }

    /* Update position */
    s_bounce_x += s_bounce_vx;
    s_bounce_y += s_bounce_vy;

    if (s_bounce_x <= 0 || s_bounce_x + sw >= SCREEN_WIDTH)  s_bounce_vx = -s_bounce_vx;
    if (s_bounce_y <= 0 || s_bounce_y + sh >= SCREEN_HEIGHT) s_bounce_vy = -s_bounce_vy;

    /* Clamp position */
    if (s_bounce_x < 0) s_bounce_x = 0;
    if (s_bounce_y < 0) s_bounce_y = 0;
    if (s_bounce_x + sw > SCREEN_WIDTH)  s_bounce_x = SCREEN_WIDTH - sw;
    if (s_bounce_y + sh > SCREEN_HEIGHT) s_bounce_y = SCREEN_HEIGHT - sh;

    /* Draw a colourful sprite: gradient-filled rect with border */
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            int idx = (s_bounce_y + y) * SCREEN_WIDTH + (s_bounce_x + x);

            /* Border */
            if (x < 2 || x >= sw - 2 || y < 2 || y >= sh - 2) {
#if defined(PIXEL_FORMAT_RGB565)
                block[idx] = RGB565_WHITE;
#else
                block[idx] = RGB888_WHITE;
#endif
            } else {
                /* Gradient fill that shifts with position */
                uint8_t r = (uint8_t)((x * 255 / sw + s_bounce_x * 2) & 0xFF);
                uint8_t g = (uint8_t)((y * 255 / sh + s_bounce_y * 2) & 0xFF);
                uint8_t b = (uint8_t)(((x + y) * 128 / (sw + sh) + s_bounce_x) & 0xFF);
                block[idx] = make_pixel(r, g, b);
            }
        }
    }
}

/* ===================================================================
 *  Unified dispatcher
 * =================================================================== */

void Effects_render(pixel_t *block, EffectMode mode, uint32_t tick)
{
    switch (mode) {
        case EFFECT_PLASMA: Effects_render_plasma(block, tick); break;
        case EFFECT_TUNNEL: Effects_render_tunnel(block, tick); break;
        case EFFECT_MOIRE:  Effects_render_moire(block, tick);  break;
        case EFFECT_FIRE:   Effects_render_fire(block, tick);   break;
        case EFFECT_BOUNCE: Effects_render_bounce(block, tick); break;
    }
}
