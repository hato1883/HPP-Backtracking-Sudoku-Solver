#ifndef SOLVER_COST_H
#define SOLVER_COST_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Representation of the cost of a move
 *
 * A move with a low cost should be better
 * than a move with a higher cost
 */
typedef struct Cost
{
    // a board can hold 1 byte of data per cell, [0, 255]
    // this gives us aproximately 255 row collissions
    // 255 column collisions, and lastly sqrt(255) block collisions,
    // this is roughly 1 040 400 collisions in the absolute worst case
    // that number fits in a 32 bit number, or 4 bytes (normal int)
    uint32_t violations;
} hpp_cost;

#endif