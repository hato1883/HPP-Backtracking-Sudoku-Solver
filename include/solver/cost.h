#ifndef __COST_H__

#include <stdlib.h>

/**
 * Representation of the cost of a move
 *
 * A move with a low cost should be better
 * than a move with a higher cost
 */
typedef struct Cost
{
    size_t violations;
} Cost;

#endif