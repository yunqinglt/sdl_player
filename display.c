/**
 * @file display.c
 * @brief Display implementation using SDL2
 *
 * Design philosophy:
 *   We decouple "writing to the framebuffer" from "flushing to the screen",
 *   mimicking how an SPI-driven LCD works:
 *
 *     SPI model:                    Our model:
 *       CASET (column addr set)  ->  SDL_UpdateTexture(dirty_rect)
 *       PASET (page addr set)    ->  (same dirty_rect)
 *       RAMWR (write RAM)        ->  SDL_RenderCopy + SDL_RenderPresent
 *
 *   The dirty region tracks which part of the framebuffer has changed
 *   since the last flush, so we only push modified pixels to the GPU.
 */

#include "display.h"




/* ===================================================================
 *  Internal helpers
 * =================================================================== */

/**
 * @brief Expand the dirty bounding rectangle to include a new region.
 */
static void dirty_union(Display *disp, int x, int y, int w, int h)
{
    if (!disp->has_dirty) {
        disp->dirty_rect.x = x;
        disp->dirty_rect.y = y;
        disp->dirty_rect.w = w;
        disp->dirty_rect.h = h;
        disp->has_dirty = true;
    } else {
        SDL_Rect new_rect;
        new_rect.x = x;
        new_rect.y = y;
        new_rect.w = w;
        new_rect.h = h;

        SDL_Rect *d = &disp->dirty_rect;
        int x1 = (d->x < new_rect.x) ? d->x : new_rect.x;
        int y1 = (d->y < new_rect.y) ? d->y : new_rect.y;
        int x2 = (d->x + d->w > new_rect.x + new_rect.w) ? (d->x + d->w) : (new_rect.x + new_rect.w);
        int y2 = (d->y + d->h > new_rect.y + new_rect.h) ? (d->y + d->h) : (new_rect.y + new_rect.h);

        d->x = x1;
        d->y = y1;
        d->w = x2 - x1;
        d->h = y2 - y1;
    }
}

/* ===================================================================
 *  Lifecycle Implementation
 * =================================================================== */

Display *Display_create(const char *title)
{
    if (title == NULL) {
        LOG_ERR("Display_create: title is NULL\n");
        return NULL;
    }

    Display *disp = (Display *)calloc(1, sizeof(Display));
    if (disp == NULL) {
        LOG_ERR("Display_create: failed to allocate Display\n");
        return NULL;
    }

    disp->width  = SCREEN_WIDTH;
    disp->height = SCREEN_HEIGHT;
    disp->has_dirty = false;
    memset(&disp->dirty_rect, 0, sizeof(disp->dirty_rect));

    /* Allocate software framebuffer */
    size_t fb_size = (size_t)disp->width * disp->height * PIXEL_BYTES;
    disp->framebuffer = (pixel_t *)malloc(fb_size);
    if (disp->framebuffer == NULL) {
        LOG_ERR("Display_create: failed to allocate framebuffer (%zu bytes)\n", fb_size);
        free(disp);
        return NULL;
    }
    memset(disp->framebuffer, 0, fb_size);

    /* Create SDL window */
    disp->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        disp->width,
        disp->height,
        SDL_WINDOW_SHOWN
    );
    if (disp->window == NULL) {
        LOG_ERR("Display_create: SDL_CreateWindow failed: %s\n", SDL_GetError());
        free(disp->framebuffer);
        free(disp);
        return NULL;
    }

    /* Create SDL renderer */
    disp->renderer = SDL_CreateRenderer(
        disp->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (disp->renderer == NULL) {
        LOG_ERR("Display_create: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(disp->window);
        free(disp->framebuffer);
        free(disp);
        return NULL;
    }

    /* Create SDL texture matching the pixel format */
    disp->texture = SDL_CreateTexture(
        disp->renderer,
        SDL_PIXEL_FORMAT,
        SDL_TEXTUREACCESS_STREAMING,
        disp->width,
        disp->height
    );
    if (disp->texture == NULL) {
        LOG_ERR("Display_create: SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(disp->renderer);
        SDL_DestroyWindow(disp->window);
        free(disp->framebuffer);
        free(disp);
        return NULL;
    }

    /* Mark full screen dirty so the first present() flushes everything */
    disp->has_dirty = true;
    disp->dirty_rect.x = 0;
    disp->dirty_rect.y = 0;
    disp->dirty_rect.w = disp->width;
    disp->dirty_rect.h = disp->height;

    LOG_INFO("Display created: %dx%d, pixel format=0x%X, %d bytes/pixel\n",
             disp->width, disp->height, SDL_PIXEL_FORMAT, (int)PIXEL_BYTES);

    return disp;
}

void Display_destroy(Display *disp)
{
    if (disp == NULL) return;

    if (disp->texture) {
        SDL_DestroyTexture(disp->texture);
    }
    if (disp->renderer) {
        SDL_DestroyRenderer(disp->renderer);
    }
    if (disp->window) {
        SDL_DestroyWindow(disp->window);
    }

    free(disp->framebuffer);
    free(disp);

    LOG_INFO("Display destroyed\n");
}

/* ===================================================================
 *  Framebuffer Writing Implementation
 * =================================================================== */

void Display_draw(Display *disp,
                  const pixel_t *block,
                  int xstart, int ystart,
                  int xend, int yend)
{
    if (disp == NULL || block == NULL) {
        LOG_ERR("Display_draw: invalid parameters (disp=%p, block=%p)\n",
                (void *)disp, (void *)block);
        return;
    }

    /* Clamp coordinates to screen bounds */
    if (xstart < 0) xstart = 0;
    if (ystart < 0) ystart = 0;
    if (xend >= disp->width)  xend = disp->width - 1;
    if (yend >= disp->height) yend = disp->height - 1;

    /* Validate region */
    if (xstart > xend || ystart > yend) {
        LOG_WARN("Display_draw: invalid region [%d,%d] -> [%d,%d]\n",
                 xstart, ystart, xend, yend);
        return;
    }

    int region_w = xend - xstart + 1;
    int region_h = yend - ystart + 1;

    /* Copy row by row from block into framebuffer (decoupled: no SDL ops) */
    for (int row = 0; row < region_h; row++) {
        int dest_y = ystart + row;
        const pixel_t *src_row = block + (size_t)row * region_w;
        pixel_t *dest_row = disp->framebuffer + (size_t)dest_y * disp->width + xstart;
        memcpy(dest_row, src_row, (size_t)region_w * PIXEL_BYTES);
    }

    /* Mark this region as dirty */
    dirty_union(disp, xstart, ystart, region_w, region_h);

    LOG_DEBUG("Display_draw: wrote (%d,%d)-(%d,%d), dirty=%s\n",
              xstart, ystart, xend, yend,
              disp->has_dirty ? "yes" : "no");
}

void Display_fill(Display *disp, pixel_t color)
{
    if (disp == NULL) return;

    size_t fb_size = (size_t)disp->width * disp->height;

    for (size_t i = 0; i < fb_size; i++) {
        disp->framebuffer[i] = color;
    }

    /* Mark entire screen dirty */
    disp->has_dirty = true;
    disp->dirty_rect.x = 0;
    disp->dirty_rect.y = 0;
    disp->dirty_rect.w = disp->width;
    disp->dirty_rect.h = disp->height;

    LOG_DEBUG("Display_fill: screen filled\n");
}

void Display_clear(Display *disp, pixel_t color)
{
    Display_fill(disp, color);
    Display_present(disp);
}

/* ===================================================================
 *  Dirty-Region Tracking Implementation
 * =================================================================== */

void Display_setDirtyRegion(Display *disp, int x, int y, int w, int h)
{
    if (disp == NULL) return;

    /* Clamp to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > disp->width)  w = disp->width - x;
    if (y + h > disp->height) h = disp->height - y;

    if (w <= 0 || h <= 0) return;

    dirty_union(disp, x, y, w, h);

    LOG_DEBUG("Display_setDirtyRegion: (%d,%d) %dx%d\n", x, y, w, h);
}

void Display_markFullDirty(Display *disp)
{
    if (disp == NULL) return;

    disp->has_dirty = true;
    disp->dirty_rect.x = 0;
    disp->dirty_rect.y = 0;
    disp->dirty_rect.w = disp->width;
    disp->dirty_rect.h = disp->height;

    LOG_DEBUG("Display_markFullDirty\n");
}

/* ===================================================================
 *  Screen Flush Implementation
 * =================================================================== */

void Display_present(Display *disp)
{
    if (disp == NULL) return;

    /* If nothing is dirty, skip the flush entirely */
    if (!disp->has_dirty) {
        LOG_DEBUG("Display_present: no dirty region, skipping\n");
        return;
    }

    SDL_Rect *d = &disp->dirty_rect;

    LOG_DEBUG("Display_present: flushing dirty region (%d,%d) %dx%d\n",
              d->x, d->y, d->w, d->h);

    /* ---- CASET + PASET analogue: update only the dirty part of the texture ---- */
    /* 
     * SDL_UpdateTexture(renderer, rect, pixels, pitch) lets us specify
     * a sub-rectangle of the texture to update — exactly like setting
     * column address (CASET) and page address (PASET) on an SPI display.
     *
     * The `pixels` pointer must point to the start of the rect's row data
     * within the framebuffer. The pitch is the full framebuffer row stride.
     */
    void *pixel_start = (void *)((uint8_t *)disp->framebuffer +
                                 (size_t)d->y * disp->width * PIXEL_BYTES +
                                 (size_t)d->x * PIXEL_BYTES);

    if (SDL_UpdateTexture(disp->texture, d, pixel_start, disp->width * PIXEL_BYTES) < 0) {
        LOG_ERR("Display_present: SDL_UpdateTexture failed: %s\n", SDL_GetError());
    }

    /* ---- RAMWR analogue: render the texture to the screen ---- */
    /* We copy the ENTIRE texture (the GPU texture holds the full image).
     * The partial update to the texture was already done above. */
    SDL_RenderClear(disp->renderer);
    SDL_RenderCopy(disp->renderer, disp->texture, NULL, NULL);
    SDL_RenderPresent(disp->renderer);

    /* Clear the dirty flag */
    disp->has_dirty = false;
    memset(&disp->dirty_rect, 0, sizeof(disp->dirty_rect));
}

/* ===================================================================
 *  Accessor Implementation
 * =================================================================== */

int Display_getWidth(const Display *disp)
{
    return (disp != NULL) ? disp->width : 0;
}

int Display_getHeight(const Display *disp)
{
    return (disp != NULL) ? disp->height : 0;
}

pixel_t *Display_getFramebuffer(Display *disp)
{
    return (disp != NULL) ? disp->framebuffer : NULL;
}

bool Display_hasDirtyRegion(const Display *disp)
{
    return (disp != NULL) && disp->has_dirty;
}

DisplayRect Display_getDirtyRegion(const Display *disp)
{
    DisplayRect r = {0, 0, 0, 0};
    if (disp != NULL && disp->has_dirty) {
        r.x = disp->dirty_rect.x;
        r.y = disp->dirty_rect.y;
        r.w = disp->dirty_rect.w;
        r.h = disp->dirty_rect.h;
    }
    return r;
}
