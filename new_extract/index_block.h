#ifndef _INDEX_BLOCK_H
#define _INDEX_BLOCK_H

#include <stdint.h>
#include "../display.h"

#ifndef BLOCK_SIZE
#define BLOCK_SIZE  40
#endif

typedef struct index_block {
    struct index_block *prev;
    struct index_block *next;

    bool dirty;

    uint16_t index;
    pixel_t *block;

} index_block_t;

index_block_t *pool_init_chain(void);

#endif