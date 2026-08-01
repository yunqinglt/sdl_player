/*
    index_block.c/h Index for framebuffer block from x, y, w, h
*/

#include "index_block.h"

#define BLOCK_COUNTS    (SCREEN_HEIGHT * SCREEN_WIDTH) / (BLOCK_SIZE * BLOCK_SIZE)

// __attribute__((section(".object_pool"))) 
static index_block_t block_pool[BLOCK_COUNTS];
static pixel_t *block[BLOCK_SIZE * BLOCK_SIZE];

index_block_t *pool_init_chain(void) {
    if (BLOCK_COUNTS == 0) return NULL;

    block_pool[0].prev = NULL;
    block_pool[0].next = (BLOCK_COUNTS > 1) ? &block_pool[1] : NULL;
    block_pool[0].index = 0;
    block_pool[0].dirty = false;

    for (uint16_t i = 1; i < BLOCK_COUNTS - 1; ++i) {
        block_pool[i].prev = &block_pool[i - 1];
        block_pool[i].next = &block_pool[i + 1];
        block_pool[i].index = i;
        block_pool[i].dirty = false;
    }

    if (BLOCK_COUNTS > 1) {
        uint16_t last = BLOCK_COUNTS - 1;
        block_pool[last].prev = &block_pool[last - 1];
        block_pool[last].next = NULL;
        block_pool[last].index = last;
        block_pool[last].dirty = false;
    }

    return &block_pool[0];
}

void alloc_from_block(index_block_t *index) {
    for (uint16_t i = 0; i < (BLOCK_SIZE * BLOCK_SIZE); ++i) {
        block[i] = 0x0000;
    }

    index->block = block;
}

void free_block(index_block_t *index) {
    index->block = NULL;
}