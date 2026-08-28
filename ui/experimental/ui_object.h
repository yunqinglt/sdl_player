/**
 * Platform-neutral high-level UI object draft.
 *
 * This replaces the syntactically incomplete ui_typedef.h while retaining its
 * intended object tree and animation concepts. It is not a committed runtime
 * API yet; see README.md.
 */

#ifndef SDL_PLAYER_EXPERIMENTAL_UI_OBJECT_H
#define SDL_PLAYER_EXPERIMENTAL_UI_OBJECT_H

#include "ui/ui_types.h"

typedef struct UiObject UiObject;
typedef struct UiAnimation UiAnimation;

typedef enum {
    UI_EVENT_TICK = 0,
    UI_EVENT_POINTER_DOWN,
    UI_EVENT_POINTER_UP,
    UI_EVENT_POINTER_MOVE,
    UI_EVENT_KEY_DOWN,
    UI_EVENT_KEY_UP
} UiEventType;

typedef struct {
    UiEventType type;
    int value1;
    int value2;
    uint32_t timestamp_ms;
} UiEvent;

typedef int32_t (*UiEasingCallback)(int32_t start, int32_t end,
                                    uint32_t elapsed_ms,
                                    uint32_t duration_ms);
typedef void (*UiAnimationApplyCallback)(UiObject *target, int32_t value);

struct UiAnimation {
    uint32_t start_time_ms;
    uint32_t duration_ms;
    int32_t start_value;
    int32_t end_value;
    UiObject *target;
    UiEasingCallback easing;
    UiAnimationApplyCallback apply;
    UiAnimation *next;
};

struct UiObject {
    UiRect base_bounds;
    UiRect bounds;
    UiAnimation *animations;
    void *widget_context;
    void *user_data;
    void (*draw)(UiObject *self, void *canvas);
    bool (*handle_event)(UiObject *self, const UiEvent *event);
    UiObject *parent;
    UiObject *first_child;
    UiObject *next_sibling;
};

#endif
