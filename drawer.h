#ifndef _DRAWER_H
#define _DRAWER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "player_conf.h"
#include "display.h"
/*
struct Display {
    SDL_Window   *window;       < SDL2 window handle
    SDL_Renderer *renderer;     < SDL2 renderer handle
    SDL_Texture  *texture;      < SDL2 texture (pixel buffer)
    pixel_t      *framebuffer;  < Software framebuffer
    int           width;        < Virtual screen width
    int           height;       < Virtual screen height

    // Dirty-region tracking
    bool          has_dirty;    < Whether any region is dirty
    SDL_Rect      dirty_rect;   < Bounding rectangle of dirty area
};
*/

typedef struct {
    int x, y;
    int w, h;
    void (*draw_cb)(struct boxed_buffer *self, void *ctx);
    void *ctx;
} ui_control_t;

typedef struct {
    const pixel_t *picture_data;
    uint16_t pic_w;
    uint16_t pic_h;
} picture_config_t;

static inline pixel_t blend_pixels(pixel_t bg, pixel_t fg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

#if defined(PIXEL_FORMAT_RGB565)
    uint16_t bg_r = (bg >> 11) & 0x1F;
    uint16_t bg_g = (bg >> 5)  & 0x3F;
    uint16_t bg_b =  bg        & 0x1F;

    uint16_t fg_r = (fg >> 11) & 0x1F;
    uint16_t fg_g = (fg >> 5)  & 0x3F;
    uint16_t fg_b =  fg        & 0x1F;

    uint16_t out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
    uint16_t out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
    uint16_t out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

    return (pixel_t)((out_r << 11) | (out_g << 5) | out_b);
#else
    uint32_t bg_r = (bg >> 16) & 0xFF;
    uint32_t bg_g = (bg >> 8)  & 0xFF;
    uint32_t bg_b =  bg        & 0xFF;

    uint32_t fg_r = (fg >> 16) & 0xFF;
    uint32_t fg_g = (fg >> 8)  & 0xFF;
    uint32_t fg_b =  fg        & 0xFF;

    uint32_t out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
    uint32_t out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
    uint32_t out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

    return (pixel_t)((out_r << 16) | (out_g << 8) | out_b);
#endif
}

typedef struct boxed_buffer {
    struct boxed_buffer *super;

    struct boxed_buffer *first_child;
    struct boxed_buffer *next_sibling;

    struct boxed_buffer *next_render;

    uint16_t refx;
    uint16_t refy;

    int w;
    int h;

    pixel_t *framebuffer;
    uint16_t stride;

    bool is_dirty;
    bool child_dirty;
    
    void (*draw_cb)(struct boxed_buffer *self, void *ctx);
    void *ctx;

} boxed_buffer_t;


typedef struct {
    uint16_t radius;
    pixel_t border_color;
    pixel_t fill_color;
    bool draw_fill;
} rounded_rect_config_t;

#if defined(PIXEL_FORMAT_RGB565)
    #define COLOR_RED        RGB565_RED
    #define COLOR_GREEN      RGB565_GREEN
    #define COLOR_DARK_GREEN RGB565(0, 40, 0)
    #define COLOR_BLACK      RGB565_BLACK
#else
    #define COLOR_RED        RGB888_RED
    #define COLOR_GREEN      RGB888_GREEN
    #define COLOR_DARK_GREEN RGB888(0, 40, 0)
    #define COLOR_BLACK      RGB888_BLACK
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))


boxed_buffer_t* isolate_from_display(Display *disp);
boxed_buffer_t* new_buffer_exact(boxed_buffer_t *super, uint16_t refx, uint16_t refy, uint16_t w, uint16_t h);
int new_buffer_fuzzy(boxed_buffer_t *super, 
                     uint16_t x1, uint16_t y1, uint16_t w1, uint16_t h1,
                     uint16_t x2, uint16_t y2, uint16_t w2, uint16_t h2,
                     uint16_t threshold,
                     boxed_buffer_t **out1, boxed_buffer_t **out2);

void draw_exact_cb(boxed_buffer_t *self, void *ctx);
void draw_fuzzy_cb(boxed_buffer_t *self, void *ctx);

void free_buffer_tree(boxed_buffer_t *node);
void link_buffer_node(boxed_buffer_t *parent, boxed_buffer_t *child);

void render_sequence(boxed_buffer_t *node);
void mark_buffer_dirty(boxed_buffer_t *bf);

static void build_render_queue(boxed_buffer_t *node, boxed_buffer_t **queue_head, boxed_buffer_t **queue_tail);
void build_smart_render_tree(boxed_buffer_t *root, ui_control_t *controls, int count, int threshold);

void draw_rounded_rect_cb(boxed_buffer_t *self, void *ctx);
void draw_picture_cb(boxed_buffer_t *self, void *ctx);


#endif