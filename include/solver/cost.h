#ifndef SOLVER_COST_H
#define SOLVER_COST_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Representation of the cost of a move with hierarchical depth weighting.
 *
 * Primary: total violations (size_t, 0 = valid solution)
 * Secondary: depth_score (double, weighted by column/block position)
 *   - Penalizes violations in leftmost columns more heavily
 *   - Penalizes violations in top-left blocks more heavily
 *   - Lower depth_score is better when violations are equal
 */
typedef struct Cost
{
    size_t violations;  // Total violations count
    double depth_score; // Weighted depth-based score (lower is better)
} hpp_cost;

#endif