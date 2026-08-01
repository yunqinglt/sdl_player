#ifndef _FRAME_H
#define _FRAME_H

#include "index_block.h"

typedef enum {
    FRAME_TYPE_SECTION = 0,
    FRAME_TYPE_DIRTY,
    FRAME_TYPE_TAB,
} frame_type_t;

typedef struct frame {
    uint16_t id; // For debug only
    frame_type_t type;

    int16_t refx;
    int16_t refy;

    int16_t w;
    int16_t h;

    uint32_t attribute1; // You can use bit operations to get attribute
    // uint32_t attribute2;
    // ...

    struct frame *super;
    struct frame *first_child;
    struct frame *next_sibling;

    index_block_t* (*dirty_render_cb) (struct frame *self, void *ctx);
    bool (*event_handler_cb) (struct frame *self, void *ctx);

} frame_object_t;


#endif