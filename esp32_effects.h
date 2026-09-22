/**
 * @file esp32_effects.h
 * @brief Ported ESP32 demo effects for sdl-player
 *
 * Ported from esp32_demo.c — renders classic demoscene-style effects:
 *   - Plasma (harmonic colour waves)
 *   - 3D Tunnel
 *   - Concentric Moire Rings
 *   - Rising Fire
 *   - Bouncing sprite/image
 *
 * All effects render into a pixel buffer that can be drawn via Display_draw().
 */

#ifndef ESP32_EFFECTS_H
#define ESP32_EFFECTS_H

#include "player_conf.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 *  Effect control / configuration
 * =================================================================== */

/** Maximum number of effect modes */
#define EFFECT_COUNT    5

/** Effect mode identifiers */
typedef enum {
    EFFECT_PLASMA = 0,
    EFFECT_TUNNEL,
    EFFECT_MOIRE,
    EFFECT_FIRE,
    EFFECT_BOUNCE
} EffectMode;

/* ===================================================================
 *  Lifecycle
 * =================================================================== */

/**
 * @brief Initialise effect tables (palettes, sine LUT, tunnel maps)
 *
 * Must be called once before any render functions.
 */
void Effects_init(void);

/**
 * @brief Re-initialise any per-effect state (e.g. fire buffer)
 *
 * Called when switching to a new effect mode.
 *
 * @param mode  The effect mode being entered
 */
void Effects_enter(EffectMode mode);

/* ===================================================================
 *  Render functions
 *
 * Each function renders the full screen into the provided @p block buffer.
 * The buffer must be at least SCREEN_WIDTH * SCREEN_HEIGHT * PIXEL_BYTES.
 *
 * @param block  Destination pixel buffer
 * @param tick   Global animation tick counter
 * =================================================================== */

/** Render classic Plasma effect */
void Effects_render_plasma(pixel_t *block, uint32_t tick);

/** Render 3D Tunnel effect */
void Effects_render_tunnel(pixel_t *block, uint32_t tick);

/** Render concentric Moire rings */
void Effects_render_moire(pixel_t *block, uint32_t tick);

/** Render rising Fire effect */
void Effects_render_fire(pixel_t *block, uint32_t tick);

/** Render bouncing "sprite" (animated rectangle) */
void Effects_render_bounce(pixel_t *block, uint32_t tick);

/* ===================================================================
 *  Unified render dispatcher
 * =================================================================== */

/**
 * @brief Render current effect mode into the pixel buffer
 *
 * Convenience wrapper that calls the appropriate render_* function
 * based on the given mode.
 *
 * @param block  Destination pixel buffer
 * @param mode   Effect mode to render
 * @param tick   Global animation tick counter
 */
void Effects_render(pixel_t *block, EffectMode mode, uint32_t tick);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_EFFECTS_H */
