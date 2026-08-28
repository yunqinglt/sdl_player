/** @file ui_types.h Platform-independent UI value types. */

#ifndef SDL_PLAYER_UI_TYPES_H
#define SDL_PLAYER_UI_TYPES_H

#include "player_conf.h"

typedef struct {
    int x;
    int y;
    int w;
    int h;
} UiRect;

static inline bool ui_rect_is_empty(UiRect rect)
{
    return rect.w <= 0 || rect.h <= 0;
}

static inline UiRect ui_rect_intersection(UiRect a, UiRect b)
{
    const int left = a.x > b.x ? a.x : b.x;
    const int top = a.y > b.y ? a.y : b.y;
    const int right_a = a.x + a.w;
    const int right_b = b.x + b.w;
    const int bottom_a = a.y + a.h;
    const int bottom_b = b.y + b.h;
    const int right = right_a < right_b ? right_a : right_b;
    const int bottom = bottom_a < bottom_b ? bottom_a : bottom_b;
    UiRect result = {left, top, right - left, bottom - top};

    if (ui_rect_is_empty(result)) {
        result.w = 0;
        result.h = 0;
    }
    return result;
}

static inline UiRect ui_rect_union(UiRect a, UiRect b)
{
    if (ui_rect_is_empty(a)) return b;
    if (ui_rect_is_empty(b)) return a;

    const int left = a.x < b.x ? a.x : b.x;
    const int top = a.y < b.y ? a.y : b.y;
    const int right_a = a.x + a.w;
    const int right_b = b.x + b.w;
    const int bottom_a = a.y + a.h;
    const int bottom_b = b.y + b.h;
    const int right = right_a > right_b ? right_a : right_b;
    const int bottom = bottom_a > bottom_b ? bottom_a : bottom_b;
    UiRect result = {left, top, right - left, bottom - top};
    return result;
}

#endif
