#include "sdl_display.h"

#include "player_conf.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <stdlib.h>

struct SdlDisplay {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    UiSurface surface;
    bool owns_sdl;
};

static Uint32 texture_format(void)
{
#if defined(PIXEL_FORMAT_RGB565)
    return SDL_PIXELFORMAT_RGB565;
#else
    return SDL_PIXELFORMAT_RGB888;
#endif
}

static void destroy_members(SdlDisplay *display)
{
    if (display->texture != NULL) SDL_DestroyTexture(display->texture);
    if (display->renderer != NULL) SDL_DestroyRenderer(display->renderer);
    if (display->window != NULL) SDL_DestroyWindow(display->window);
    ui_surface_destroy(&display->surface);
    if (display->owns_sdl) SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER);
}

SdlDisplay *sdl_display_create(const char *title, int width, int height)
{
    SdlDisplay *display;
    const Uint32 required = SDL_INIT_VIDEO | SDL_INIT_TIMER;

    if (title == NULL || width <= 0 || height <= 0) return NULL;
    SDL_SetMainReady();
    display = calloc(1, sizeof(*display));
    if (display == NULL) return NULL;

    if ((SDL_WasInit(required) & required) != required) {
        if (SDL_InitSubSystem(required) < 0) {
            LOG_ERROR("SDL initialization failed: %s\n", SDL_GetError());
            free(display);
            return NULL;
        }
        display->owns_sdl = true;
    }

    if (!ui_surface_create(&display->surface, width, height)) {
        LOG_ERROR("Could not allocate the %dx%d framebuffer\n", width, height);
        destroy_members(display);
        free(display);
        return NULL;
    }

    display->window = SDL_CreateWindow(title,
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        width, height,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (display->window == NULL) {
        LOG_ERROR("SDL_CreateWindow failed: %s\n", SDL_GetError());
        destroy_members(display);
        free(display);
        return NULL;
    }

    display->renderer = SDL_CreateRenderer(display->window, -1,
                                            SDL_RENDERER_ACCELERATED |
                                            SDL_RENDERER_PRESENTVSYNC);
    if (display->renderer == NULL) {
        LOG_WARN("Accelerated renderer unavailable (%s); using software\n",
                 SDL_GetError());
        display->renderer = SDL_CreateRenderer(display->window, -1,
                                                SDL_RENDERER_SOFTWARE);
    }
    if (display->renderer == NULL) {
        LOG_ERROR("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        destroy_members(display);
        free(display);
        return NULL;
    }
    (void)SDL_RenderSetLogicalSize(display->renderer, width, height);

    display->texture = SDL_CreateTexture(display->renderer, texture_format(),
                                         SDL_TEXTUREACCESS_STREAMING,
                                         width, height);
    if (display->texture == NULL) {
        LOG_ERROR("SDL_CreateTexture failed: %s\n", SDL_GetError());
        destroy_members(display);
        free(display);
        return NULL;
    }

    LOG_INFO("SDL screen created: %dx%d, %d bytes/pixel\n",
             width, height, PIXEL_BYTES);
    return display;
}

void sdl_display_destroy(SdlDisplay *display)
{
    if (display == NULL) return;
    destroy_members(display);
    free(display);
}

UiSurface *sdl_display_surface(SdlDisplay *display)
{
    return display != NULL ? &display->surface : NULL;
}

bool sdl_display_present(SdlDisplay *display)
{
    UiRect dirty;
    SDL_Rect sdl_dirty;
    const pixel_t *start;

    if (display == NULL) return false;
    if (!display->surface.has_dirty) return true;

    dirty = ui_surface_dirty_rect(&display->surface);
    sdl_dirty = (SDL_Rect){dirty.x, dirty.y, dirty.w, dirty.h};
    start = display->surface.pixels +
        (size_t)dirty.y * (size_t)display->surface.stride + (size_t)dirty.x;

    if (SDL_UpdateTexture(display->texture, &sdl_dirty, start,
                          display->surface.stride * PIXEL_BYTES) < 0) {
        LOG_ERROR("SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_RenderClear(display->renderer) < 0 ||
        SDL_RenderCopy(display->renderer, display->texture, NULL, NULL) < 0) {
        LOG_ERROR("SDL render failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_RenderPresent(display->renderer);
    ui_surface_clear_dirty(&display->surface);
    return true;
}

SdlDisplayEvent sdl_display_poll_event(SdlDisplay *display)
{
    SDL_Event event;
    (void)display;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return SDL_DISPLAY_EVENT_QUIT;
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_ESCAPE) return SDL_DISPLAY_EVENT_QUIT;
            if (event.key.keysym.sym == SDLK_SPACE) {
                return SDL_DISPLAY_EVENT_NEXT_DEMO;
            }
        }
    }
    return SDL_DISPLAY_EVENT_NONE;
}

uint32_t sdl_display_ticks(void)
{
    return SDL_GetTicks();
}

uint64_t sdl_display_microseconds(void)
{
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    return frequency == 0 ? 0 : SDL_GetPerformanceCounter() * 1000000u / frequency;
}

void sdl_display_delay(uint32_t milliseconds)
{
    SDL_Delay(milliseconds);
}
