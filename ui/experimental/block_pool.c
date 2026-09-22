#include "block_pool.h"

#include <stddef.h>
#include <string.h>

static UiIndexBlock blocks[UI_BLOCK_COUNT];
static pixel_t storage[UI_BLOCK_COUNT][UI_BLOCK_SIZE * UI_BLOCK_SIZE];

UiIndexBlock *ui_block_pool_init(void)
{
    for (size_t i = 0; i < UI_BLOCK_COUNT; ++i) {
        blocks[i].previous = i == 0 ? NULL : &blocks[i - 1];
        blocks[i].next = i + 1 == UI_BLOCK_COUNT ? NULL : &blocks[i + 1];
        blocks[i].dirty = false;
        blocks[i].index = (uint16_t)i;
        blocks[i].pixels = NULL;
    }
    return UI_BLOCK_COUNT == 0 ? NULL : &blocks[0];
}

pixel_t *ui_block_acquire(UiIndexBlock *block)
{
    if (block == NULL || block->index >= UI_BLOCK_COUNT) return NULL;
    block->pixels = storage[block->index];
    memset(block->pixels, 0,
           UI_BLOCK_SIZE * UI_BLOCK_SIZE * sizeof(*block->pixels));
    return block->pixels;
}

void ui_block_release(UiIndexBlock *block)
{
    if (block == NULL) return;
    block->pixels = NULL;
    block->dirty = false;
}
