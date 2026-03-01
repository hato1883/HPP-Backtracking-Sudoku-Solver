#ifndef SOLVER_INCREMENTAL_COST_H
#define SOLVER_INCREMENTAL_COST_H

#include "solver/cost.h"
#include "solver/move.h"
#include "solver/sudoku.h"

/**
 * Incremental cost breakdown: tracks violations per column and block.
 * Rows are always valid (no duplicates) due to row-swap moves and proper initialization.
 * Allows O(1) cost delta computation for moves without board modification.
 */
typedef struct IncrementalCost
{
    /** Violations per column. */
    uint32_t* col_costs;

    /** Violations per block (2D: block_row, block_col). */
    uint32_t** block_costs;

    /** Total violations (sum of all col, block costs). */
    uint32_t total_violations;

    /** Board size. */
    size_t size;

    /** Block size. */
    size_t block_size;
} hpp_incremental_cost;

/**
 * Initialize incremental cost table from board.
 * Allocates arrays and computes initial cost breakdown.
 *
 * @param board the board to analyze
 * @return newly allocated cost table, or NULL on failure
 */
hpp_incremental_cost* incremental_cost_init(const hpp_board* board);

/**
 * Destroy incremental cost table and free resources.
 *
 * @param cost the cost table to destroy (pointer is set to NULL)
 */
void incremental_cost_destroy(hpp_incremental_cost** cost);

/**
 * Compute the cost delta (change) for assigning a new value to a cell.
 * Does NOT modify the board or cost table.
 *
 * @param cost current incremental cost
 * @param board the board (used to look up current values)
 * @param row target row
 * @param col target column
 * @param new_value the value to assign
 * @return cost delta (positive means cost increases, negative means decreases)
 */
int32_t incremental_cost_delta_assign(const hpp_incremental_cost* cost,
                                      const hpp_board*            board,
                                      size_t                      row,
                                      size_t                      col,
                                      uint8_t                     new_value);

/**
 * Compute the cost delta for swapping two cells.
 * Does NOT modify the board or cost table.
 *
 * @param cost current incremental cost
 * @param board the board (used to look up current values)
 * @param row1 first cell row
 * @param col1 first cell column
 * @param row2 second cell row
 * @param col2 second cell column
 * @return cost delta
 */
int32_t incremental_cost_delta_swap(const hpp_incremental_cost* cost,
                                    const hpp_board*            board,
                                    size_t                      row1,
                                    size_t                      col1,
                                    size_t                      row2,
                                    size_t                      col2);

/**
 * Apply a move to the incremental cost table (updates the breakdown).
 * Must be called after a move is applied to the board.
 *
 * @param cost the cost table to update
 * @param board the board (after move is applied)
 * @param move the move that was applied
 */
void incremental_cost_apply_move(hpp_incremental_cost* cost,
                                 const hpp_board*      board,
                                 const hpp_move*       move);

#endif
