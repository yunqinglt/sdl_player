/** @file demo.h Platform-independent demo application state. */

#ifndef SDL_PLAYER_DEMO_H
#define SDL_PLAYER_DEMO_H

#include "ui/ui_surface.h"

#include <stddef.h>
#include <stdint.h>

#define DEMO_PHASE_COUNT 9

typedef struct {
    UiSurface *surface;
    unsigned phase;
    uint32_t tick;
    int rect_x;
    int rect_y;
    int rect_dx;
    int rect_dy;
    bool ui_tree_debug;
    bool ui_tree_debug_has_topology;
    uint64_t ui_tree_debug_topology_hash;
    size_t ui_tree_debug_group_count;
} Demo;

bool demo_init(Demo *demo, UiSurface *surface);
void demo_set_ui_tree_debug(Demo *demo, bool enabled);
void demo_set_phase(Demo *demo, unsigned phase);
void demo_next_phase(Demo *demo);
const char *demo_phase_name(const Demo *demo);
bool demo_render(Demo *demo);

#endif
