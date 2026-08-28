/** @file main.c Thin PC application glue: SDL backend + portable demo/UI. */

#include "app/demo.h"
#include "platform/sdl/sdl_display.h"
#include "player_conf.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t frame_limit;
    unsigned initial_phase;
} Options;

static void print_usage(const char *program)
{
    printf("Usage: %s [--frames N] [--phase 0-%d]\n", program,
           DEMO_PHASE_COUNT - 1);
    printf("  SPACE: next demo    ESC: quit\n");
    printf("  --frames is useful for automated/headless smoke tests.\n");
}

static bool parse_unsigned(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || *text == '\0' || *text == '-') return false;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = (uint64_t)parsed;
    return true;
}

static int parse_options(int argc, char **argv, Options *options)
{
    *options = (Options){0};
    for (int i = 1; i < argc; ++i) {
        uint64_t value;

        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 1;
        }
        if ((strcmp(argv[i], "--frames") == 0 ||
             strcmp(argv[i], "--phase") == 0) && i + 1 < argc) {
            const bool is_phase = strcmp(argv[i], "--phase") == 0;
            if (!parse_unsigned(argv[++i], &value) ||
                (is_phase && value >= DEMO_PHASE_COUNT)) {
                LOG_ERROR("Invalid value for %s\n", argv[i - 1]);
                return -1;
            }
            if (is_phase) options->initial_phase = (unsigned)value;
            else options->frame_limit = value;
            continue;
        }
        LOG_ERROR("Unknown or incomplete option: %s\n", argv[i]);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    Options options;
    SdlDisplay *display;
    Demo demo;
    bool running = true;
    bool failed = false;
    uint64_t rendered_frames = 0;
    uint32_t fps_started;
    uint32_t fps_frames = 0;
    int option_result = parse_options(argc, argv, &options);

    if (option_result != 0) {
        if (option_result < 0) print_usage(argv[0]);
        return option_result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

#if defined(PIXEL_FORMAT_RGB565)
    LOG_INFO("sdl-player starting (%dx%d RGB565)\n", SCREEN_WIDTH, SCREEN_HEIGHT);
#else
    LOG_INFO("sdl-player starting (%dx%d RGB888)\n", SCREEN_WIDTH, SCREEN_HEIGHT);
#endif

    display = sdl_display_create("sdl-player: virtual embedded screen",
                                 SCREEN_WIDTH, SCREEN_HEIGHT);
    if (display == NULL) return EXIT_FAILURE;
    if (!demo_init(&demo, sdl_display_surface(display))) {
        LOG_ERROR("Demo initialization failed\n");
        sdl_display_destroy(display);
        return EXIT_FAILURE;
    }
    demo_set_phase(&demo, options.initial_phase);
    LOG_INFO("Demo %u: %s\n", demo.phase, demo_phase_name(&demo));
    fps_started = sdl_display_ticks();

    while (running &&
           (options.frame_limit == 0 || rendered_frames < options.frame_limit)) {
        const uint32_t frame_started = sdl_display_ticks();
        SdlDisplayEvent event;

        while ((event = sdl_display_poll_event(display)) !=
               SDL_DISPLAY_EVENT_NONE) {
            if (event == SDL_DISPLAY_EVENT_QUIT) {
                running = false;
            } else if (event == SDL_DISPLAY_EVENT_NEXT_DEMO) {
                demo_next_phase(&demo);
                LOG_INFO("Demo %u: %s\n", demo.phase, demo_phase_name(&demo));
            }
        }
        if (!running) break;

        if (!demo_render(&demo) || !sdl_display_present(display)) {
            LOG_ERROR("Rendering stopped after %llu frame(s)\n",
                      (unsigned long long)rendered_frames);
            running = false;
            failed = true;
            break;
        }

        ++rendered_frames;
        ++fps_frames;
#if FPS_ENABLE
        {
            const uint32_t now = sdl_display_ticks();
            const uint32_t elapsed = now - fps_started;
            if (elapsed >= 1000u) {
                LOG_INFO("FPS: %.1f\n", (double)fps_frames * 1000.0 / elapsed);
                fps_frames = 0;
                fps_started = now;
            }
        }
#endif

#if TARGET_FPS > 0
        {
            const uint32_t elapsed = sdl_display_ticks() - frame_started;
            const uint32_t target = 1000u / TARGET_FPS;
            if (elapsed < target) sdl_display_delay(target - elapsed);
        }
#else
        (void)frame_started;
#endif
    }

    sdl_display_destroy(display);
    LOG_INFO("sdl-player exited after %llu frame(s)\n",
             (unsigned long long)rendered_frames);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
