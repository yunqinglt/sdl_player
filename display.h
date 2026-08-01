/**
 * @file display.h
 * @brief Display abstraction layer for sdl-player
 *
 * Provides a Display object that wraps an SDL window, renderer, and texture.
 * The design decouples software framebuffer writes from hardware (texture)
 * flushes, mimicking how an SPI-controlled display works with column/page
 * address registers for partial updates.
 *
 * Typical usage pattern (mimicking SPI display register model):
 *
 *   1. Write pixel data into the framebuffer via Display_draw() or
 *      Display_getFramebuffer() direct access.
 *   2. Optionally set a dirty region via Display_setDirtyRegion().
 *   3. Call Display_present() to flush only the dirty region to the
 *      SDL texture, just like sending column/page address commands + RAMWR.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "player_conf.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 *  Display Structure Definition
 * =================================================================== */

struct Display {
    SDL_Window   *window;       /**< SDL2 window handle              */
    SDL_Renderer *renderer;     /**< SDL2 renderer handle            */
    SDL_Texture  *texture;      /**< SDL2 texture (pixel buffer)     */
    pixel_t      *framebuffer;  /**< Software framebuffer            */
    int           width;        /**< Virtual screen width            */
    int           height;       /**< Virtual screen height           */

    /* Dirty-region tracking */
    bool          has_dirty;    /**< Whether any region is dirty     */
    SDL_Rect      dirty_rect;   /**< Bounding rectangle of dirty area */
};

/* ===================================================================
 *  Display Structure (opaque front-end type)
 * =================================================================== */

/**
 * @brief Display structure
 *
 * The Display object manages the SDL window, renderer, texture, and the
 * software framebuffer. The internal details are hidden in display.c;
 * users interact only through the exported functions.
 */
typedef struct Display Display;

/* ===================================================================
 *  Rect (used for dirty-region tracking)
 * =================================================================== */

/**
 * @brief Simple 2D rectangle
 */
typedef struct {
    int x;      /**< X origin (left)   */
    int y;      /**< Y origin (top)    */
    int w;      /**< Width             */
    int h;      /**< Height            */
} DisplayRect;

/* ===================================================================
 *  Lifecycle Functions
 * =================================================================== */

/**
 * @brief Create and initialise a new Display instance
 *
 * Opens an SDL2 window, creates a renderer and a texture matching the
 * virtual screen configuration defined in player_conf.h.
 *
 * @param title  Window title string
 * @return       Pointer to a new Display, or NULL on failure.
 */
Display *Display_create(const char *title);

/**
 * @brief Destroy a Display instance and free all resources
 *
 * @param disp  Pointer to the Display to destroy
 */
void Display_destroy(Display *disp);

/* ===================================================================
 *  Framebuffer Writing (analogous to SPI "write pixels into GRAM")
 * =================================================================== */

/**
 * @brief Write a block of pixel data into the software framebuffer
 *
 * Copies a rectangular region from the provided @p block buffer into the
 * display's internal framebuffer at the specified area. This operation
 * is purely a memory write — it does NOT touch the SDL texture or screen.
 *
 * The @p block must be a contiguous array of `pixel_t` values with
 * dimensions at least (xend - xstart + 1) wide and (yend - ystart + 1)
 * tall.
 *
 * Any region written by this call is automatically marked as "dirty" and
 * will be flushed to the screen on the next Display_present() call.
 *
 * @param disp    Pointer to the Display
 * @param block   Pointer to the source pixel data buffer
 * @param xstart  Starting X coordinate (inclusive) of the destination region
 * @param ystart  Starting Y coordinate (inclusive) of the destination region
 * @param xend    Ending X coordinate (inclusive) of the destination region
 * @param yend    Ending Y coordinate (inclusive) of the destination region
 */
void Display_draw(Display *disp,
                  const pixel_t *block,
                  int xstart, int ystart,
                  int xend, int yend);

/**
 * @brief Fill the entire framebuffer with a single colour
 *
 * This does NOT flush to the screen. Call Display_present() afterwards.
 *
 * @param disp  Pointer to the Display
 * @param color The colour value to fill with
 */
void Display_fill(Display *disp, pixel_t color);

/**
 * @brief Convenience: fill framebuffer and present immediately
 *
 * Equivalent to Display_fill() + Display_present().
 *
 * @param disp  Pointer to the Display
 * @param color The colour value to fill with
 */
void Display_clear(Display *disp, pixel_t color);

/* ===================================================================
 *  Dirty-Region Tracking / Partial Flush (analogous to SPI CASET+PASET)
 * =================================================================== */

/**
 * @brief Manually mark a rectangular region as dirty
 *
 * Forces the next Display_present() to flush only this region to the
 * SDL texture, regardless of how the framebuffer was modified.
 *
 * @param disp  Pointer to the Display
 * @param x     Left coordinate
 * @param y     Top coordinate
 * @param w     Width of the region
 * @param h     Height of the region
 */
void Display_setDirtyRegion(Display *disp, int x, int y, int w, int h);

/**
 * @brief Mark the entire screen as dirty
 *
 * Forces the next Display_present() to flush the whole framebuffer.
 *
 * @param disp  Pointer to the Display
 */
void Display_markFullDirty(Display *disp);

/* ===================================================================
 *  Screen Flush (analogous to SPI "send CASET+PASET+RAMWR")
 * =================================================================== */

/**
 * @brief Flush the dirty region(s) to the SDL texture and present to screen
 *
 * Analogy to SPI display driving:
 *   - Sets column address (CASET)  = SDL_UpdateTexture with dirty rect
 *   - Sets page address (PASET)    = (same rect)
 *   - Writes RAM (RAMWR)            = SDL_RenderCopy + SDL_RenderPresent
 *
 * After flushing, the dirty region is reset to empty.
 *
 * @param disp  Pointer to the Display
 */
void Display_present(Display *disp);

/* ===================================================================
 *  Accessors
 * =================================================================== */

/**
 * @brief Get the width of the virtual screen
 *
 * @param disp  Pointer to the Display
 * @return      Width in pixels
 */
int Display_getWidth(const Display *disp);

/**
 * @brief Get the height of the virtual screen
 *
 * @param disp  Pointer to the Display
 * @return      Height in pixels
 */
int Display_getHeight(const Display *disp);

/**
 * @brief Get a pointer to the internal software framebuffer
 *
 * Useful for direct pixel manipulation. After modifying the buffer,
 * call Display_present() to flush changes to the screen, or use
 * Display_setDirtyRegion() to mark the modified area manually.
 *
 * @param disp  Pointer to the Display
 * @return      Pointer to the framebuffer array (pixel_t)
 */
pixel_t *Display_getFramebuffer(Display *disp);

/**
 * @brief Query whether the display has pending dirty regions
 *
 * @param disp  Pointer to the Display
 * @return      true if there are unflushed changes
 */
bool Display_hasDirtyRegion(const Display *disp);

/**
 * @brief Get the current bounding dirty rectangle
 *
 * Returns the smallest rectangle that contains all currently dirty areas.
 * If no dirty region exists, returns a zero-sized rectangle.
 *
 * @param disp  Pointer to the Display
 * @return      Bounding dirty rectangle
 */
DisplayRect Display_getDirtyRegion(const Display *disp);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_H */
