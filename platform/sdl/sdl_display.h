/** @file sdl_display.h Windows/Linux PC screen backend. */

#ifndef SDL_PLAYER_SDL_DISPLAY_H
#define SDL_PLAYER_SDL_DISPLAY_H

#include "ui/ui_surface.h"

#include <stdint.h>

typedef struct SdlDisplay SdlDisplay;

typedef enum {
    SDL_DISPLAY_EVENT_NONE = 0,
    SDL_DISPLAY_EVENT_QUIT,
    SDL_DISPLAY_EVENT_NEXT_DEMO
} SdlDisplayEvent;

SdlDisplay *sdl_display_create(const char *title, int width, int height);
void sdl_display_destroy(SdlDisplay *display);
UiSurface *sdl_display_surface(SdlDisplay *display);
bool sdl_display_present(SdlDisplay *display);
SdlDisplayEvent sdl_display_poll_event(SdlDisplay *display);
uint32_t sdl_display_ticks(void);
uint64_t sdl_display_microseconds(void);
void sdl_display_delay(uint32_t milliseconds);

#endif
