/**
 * @file player_conf.h
 * @brief Platform-independent framebuffer configuration.
 *
 * This header deliberately contains no SDL declarations. It can be reused by
 * an MCU display driver and by the PC simulator.
 */

#ifndef SDL_PLAYER_CONF_H
#define SDL_PLAYER_CONF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 800
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 600
#endif

/* Define PIXEL_FORMAT_RGB888 from CMake to select the 32-bit format. */
#if !defined(PIXEL_FORMAT_RGB565) && !defined(PIXEL_FORMAT_RGB888)
#define PIXEL_FORMAT_RGB565
#endif

#if defined(PIXEL_FORMAT_RGB565) && defined(PIXEL_FORMAT_RGB888)
#error "Select only one pixel format"
#endif

#if defined(PIXEL_FORMAT_RGB565)
typedef uint16_t pixel_t;
#define PIXEL_BYTES ((int)sizeof(pixel_t))
#define RGB565(r, g, b) ((pixel_t)((((uint16_t)(r) & 0xf8u) << 8) | \
                                  (((uint16_t)(g) & 0xfcu) << 3) | \
                                  (((uint16_t)(b) & 0xf8u) >> 3)))
#define PIXEL_RGB(r, g, b) RGB565((r), (g), (b))
#define RGB565_RED       RGB565(255, 0, 0)
#define RGB565_GREEN     RGB565(0, 255, 0)
#define RGB565_BLUE      RGB565(0, 0, 255)
#define RGB565_WHITE     RGB565(255, 255, 255)
#define RGB565_BLACK     RGB565(0, 0, 0)
#define RGB565_YELLOW    RGB565(255, 255, 0)
#define RGB565_CYAN      RGB565(0, 255, 255)
#else
typedef uint32_t pixel_t;
#define PIXEL_BYTES ((int)sizeof(pixel_t))
#define RGB888(r, g, b) ((pixel_t)((((uint32_t)(r) & 0xffu) << 16) | \
                                  (((uint32_t)(g) & 0xffu) << 8) | \
                                  ((uint32_t)(b) & 0xffu)))
#define PIXEL_RGB(r, g, b) RGB888((r), (g), (b))
#define RGB888_RED       RGB888(255, 0, 0)
#define RGB888_GREEN     RGB888(0, 255, 0)
#define RGB888_BLUE      RGB888(0, 0, 255)
#define RGB888_WHITE     RGB888(255, 255, 255)
#define RGB888_BLACK     RGB888(0, 0, 0)
#define RGB888_YELLOW    RGB888(255, 255, 0)
#define RGB888_CYAN      RGB888(0, 255, 255)
#endif

#define COLOR_RED        PIXEL_RGB(255, 0, 0)
#define COLOR_GREEN      PIXEL_RGB(0, 255, 0)
#define COLOR_BLUE       PIXEL_RGB(0, 0, 255)
#define COLOR_WHITE      PIXEL_RGB(255, 255, 255)
#define COLOR_BLACK      PIXEL_RGB(0, 0, 0)
#define COLOR_YELLOW     PIXEL_RGB(255, 255, 0)
#define COLOR_CYAN       PIXEL_RGB(0, 255, 255)
#define COLOR_DARK_GREEN PIXEL_RGB(0, 40, 0)

#define FRAMEBUFFER_SIZE ((size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(pixel_t))

#ifndef LOG_ENABLE
#define LOG_ENABLE 1
#endif

#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if LOG_ENABLE
#include <stdio.h>
#define LOG_WRITE(level, ...) do { \
    (void)fprintf(stdout, "[%s] ", (level)); \
    (void)fprintf(stdout, __VA_ARGS__); \
} while (0)
#else
#define LOG_WRITE(level, ...) ((void)0)
#endif

#if LOG_ENABLE && LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) LOG_WRITE("DEBUG", __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

#if LOG_ENABLE && LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(...) LOG_WRITE("INFO", __VA_ARGS__)
#else
#define LOG_INFO(...) ((void)0)
#endif

#if LOG_ENABLE && LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(...) LOG_WRITE("WARN", __VA_ARGS__)
#else
#define LOG_WARN(...) ((void)0)
#endif

#if LOG_ENABLE && LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(...) LOG_WRITE("ERROR", __VA_ARGS__)
#else
#define LOG_ERROR(...) ((void)0)
#endif

/* Compatibility with the old spelling. */
#define LOG_ERR(...) LOG_ERROR(__VA_ARGS__)

#ifndef FPS_ENABLE
#define FPS_ENABLE 1
#endif

#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

#endif
