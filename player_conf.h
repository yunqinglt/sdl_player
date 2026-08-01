/**
 * @file player_conf.h
 * @brief Virtual hardware configuration for sdl-player
 *
 * This file defines the virtual hardware configuration for the SDL-based
 * embedded display player. It sets screen resolution, color mode,
 * console logging, and frame rate recording options.
 */

#ifndef PLAYER_CONF_H
#define PLAYER_CONF_H

#include <stdint.h>
#include <stdbool.h>

/* ===================================================================
 *  Virtual Screen Configuration
 * =================================================================== */

/** Virtual screen width (pixels) */
#define SCREEN_WIDTH        800

/** Virtual screen height (pixels) */
#define SCREEN_HEIGHT       600

/**
 * @brief Pixel format selection
 *
 * Define exactly ONE of the following to select the color mode:
 *   - PIXEL_FORMAT_RGB565  : 16-bit RGB (5-bit R, 6-bit G, 5-bit B)
 *   - PIXEL_FORMAT_RGB888  : 32-bit RGB (8-bit R, 8-bit G, 8-bit B, 8-bit unused/padding)
 */
#define PIXEL_FORMAT_RGB565
/* #define PIXEL_FORMAT_RGB888 */

/* ===================================================================
 *  Console / Logging Configuration
 * =================================================================== */

/** Enable or disable console output logging */
#define LOG_ENABLE          true

/**
 * Log level definitions
 * LOG_LEVEL_NONE:  No output
 * LOG_LEVEL_ERR:   Only errors
 * LOG_LEVEL_WARN:  Errors and warnings
 * LOG_LEVEL_INFO:  Normal information (default)
 * LOG_LEVEL_DEBUG: Detailed debug messages
 */
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERR   1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

/** Set the active log level */
#define LOG_LEVEL           LOG_LEVEL_INFO

/* ===================================================================
 *  Frame Rate Recording Configuration
 * =================================================================== */

/** Enable frame rate statistics (FPS counter) */
#define FPS_ENABLE          true

/** How often (in frames) to print FPS to console */
#define FPS_PRINT_INTERVAL  60

/** Target frame rate (0 = unlimited) */
#define TARGET_FPS          0

/* ===================================================================
 *  Derived types based on color mode
 * =================================================================== */

#if defined(PIXEL_FORMAT_RGB565)
    /** Pixel type: 16-bit RGB565 */
    typedef uint16_t pixel_t;
    #define PIXEL_BYTES     2
    #define SDL_PIXEL_FORMAT SDL_PIXELFORMAT_RGB565

    /** Helper macros for RGB565 color packing */
    #define RGB565(r, g, b)  ((pixel_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
    #define RGB565_RED       RGB565(255, 0, 0)
    #define RGB565_GREEN     RGB565(0, 255, 0)
    #define RGB565_BLUE      RGB565(0, 0, 255)
    #define RGB565_WHITE     RGB565(255, 255, 255)
    #define RGB565_BLACK     RGB565(0, 0, 0)
    #define RGB565_YELLOW    RGB565(255, 255, 0)
    #define RGB565_CYAN      RGB565(0, 255, 255)

#elif defined(PIXEL_FORMAT_RGB888)
    /** Pixel type: 32-bit RGB888 (unused alpha byte for alignment) */
    typedef uint32_t pixel_t;
    #define PIXEL_BYTES      4
    #define SDL_PIXEL_FORMAT SDL_PIXELFORMAT_RGB888

    /** Helper macros for RGB888 color packing */
    #define RGB888(r, g, b)  ((pixel_t)((r << 16) | (g << 8) | b))
    #define RGB888_RED       RGB888(255, 0, 0)
    #define RGB888_GREEN     RGB888(0, 255, 0)
    #define RGB888_BLUE      RGB888(0, 0, 255)
    #define RGB888_WHITE     RGB888(255, 255, 255)
    #define RGB888_BLACK     RGB888(0, 0, 0)
    #define RGB888_YELLOW    RGB888(255, 255, 0)
    #define RGB888_CYAN      RGB888(0, 255, 255)

#else
    #error "No pixel format defined! Define either PIXEL_FORMAT_RGB565 or PIXEL_FORMAT_RGB888."
#endif

/** Total framebuffer size in bytes */
#define FRAMEBUFFER_SIZE    (SCREEN_WIDTH * SCREEN_HEIGHT * PIXEL_BYTES)

/* ===================================================================
 *  Logging Macros
 * =================================================================== */

#if LOG_ENABLE

#include <stdio.h>

/** @cond INTERNAL */
#define LOG_PREFIX(level, fmt)  printf("[%s] ", level); printf(fmt)
#define LOG_PREFIX_ARGS(level, fmt, ...)  printf("[%s] ", level); printf(fmt, __VA_ARGS__)
/** @endcond */

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(fmt, ...)    LOG_PREFIX_ARGS("DEBUG", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...)    ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
    #define LOG_INFO(fmt, ...)     LOG_PREFIX_ARGS("INFO", fmt, ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
    #define LOG_WARN(fmt, ...)     LOG_PREFIX_ARGS("WARN", fmt, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERR
    #define LOG_ERR(fmt, ...)      LOG_PREFIX_ARGS("ERR", fmt, ##__VA_ARGS__)
#else
    #define LOG_ERR(fmt, ...)      ((void)0)
#endif

#else /* LOG_ENABLE */

#define LOG_DEBUG(fmt, ...)  ((void)0)
#define LOG_INFO(fmt, ...)   ((void)0)
#define LOG_WARN(fmt, ...)   ((void)0)
#define LOG_ERR(fmt, ...)    ((void)0)

#endif /* LOG_ENABLE */

#endif /* PLAYER_CONF_H */
