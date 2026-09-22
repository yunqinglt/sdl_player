/** @file block_pool.h Experimental fixed-size framebuffer block pool. */

#ifndef SDL_PLAYER_EXPERIMENTAL_BLOCK_POOL_H
#define SDL_PLAYER_EXPERIMENTAL_BLOCK_POOL_H

#include "player_conf.h"

#ifndef UI_BLOCK_SIZE
#define UI_BLOCK_SIZE 40
#endif

#define UI_BLOCK_COUNT \
    (((SCREEN_WIDTH + UI_BLOCK_SIZE - 1) / UI_BLOCK_SIZE) * \
     ((SCREEN_HEIGHT + UI_BLOCK_SIZE - 1) / UI_BLOCK_SIZE))

typedef struct UiIndexBlock {
    struct UiIndexBlock *previous;
    struct UiIndexBlock *next;
    bool dirty;
    uint16_t index;
    pixel_t *pixels;
} UiIndexBlock;

UiIndexBlock *ui_block_pool_init(void);
pixel_t *ui_block_acquire(UiIndexBlock *block);
void ui_block_release(UiIndexBlock *block);

#endif
