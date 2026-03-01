#include "solver/incremental_cost.h"

#include "solver/cost.h"
#include "utils/logger.h"

#include <stdlib.h>
#include <string.h>

hpp_incremental_cost* incremental_cost_init(const hpp_board* board)
{
    if (board == NULL || board->block_size == 0)
    {
        return NULL;
    }

    hpp_incremental_cost* cost = (hpp_incremental_cost*)malloc(sizeof(*cost));
    if (cost == NULL)
    {
        return NULL;
    }

    cost->size       = board->size;
    cost->block_size = board->block_size;

    // Allocate column costs
    cost->col_costs = (uint32_t*)malloc(sizeof(uint32_t) * board->size);
    if (cost->col_costs == NULL)
    {
        free(cost);
        return NULL;
    }

    // Allocate block costs (2D array)
    size_t num_blocks = board->size / board->block_size;
    cost->block_costs = (uint32_t**)malloc(sizeof(uint32_t*) * num_blocks);
    if (cost->block_costs == NULL)
    {
        free(cost->col_costs);
        free(cost);
        return NULL;
    }

    for (size_t i = 0; i < num_blocks; ++i)
    {
        cost->block_costs[i] = (uint32_t*)malloc(sizeof(uint32_t) * num_blocks);
        if (cost->block_costs[i] == NULL)
        {
            for (size_t j = 0; j < i; ++j)
            {
                free(cost->block_costs[j]);
            }
            free(cost->block_costs);
            free(cost->col_costs);
            free(cost);
            return NULL;
        }
    }

    // Compute column and block costs (rows always valid, no need to track)
    uint32_t total = 0;

    // Compute column costs
    for (size_t col = 0; col < board->size; ++col)
    {
        cost->col_costs[col] = count_conflicts_in_col(board, col);
        total += cost->col_costs[col];
    }

    // Compute block costs
    for (size_t block_row = 0; block_row < num_blocks; ++block_row)
    {
        for (size_t block_col = 0; block_col < num_blocks; ++block_col)
        {
            cost->block_costs[block_row][block_col] =
                count_conflicts_in_block(board, block_row, block_col);
            total += cost->block_costs[block_row][block_col];
        }
    }

    cost->total_violations = total;

    LOG_DEBUG("Incremental cost initialized: %u violations", total);
    return cost;
}

void incremental_cost_destroy(hpp_incremental_cost** cost_ptr)
{
    if (cost_ptr == NULL || *cost_ptr == NULL)
    {
        return;
    }

    hpp_incremental_cost* cost = *cost_ptr;

    if (cost->col_costs != NULL)
        free(cost->col_costs);

    if (cost->block_costs != NULL)
    {
        size_t num_blocks = cost->size / cost->block_size;
        for (size_t i = 0; i < num_blocks; ++i)
        {
            if (cost->block_costs[i] != NULL)
                free(cost->block_costs[i]);
        }
        free(cost->block_costs);
    }

    free(cost);
    *cost_ptr = NULL;
}

int32_t incremental_cost_delta_assign(const hpp_incremental_cost* cost,
                                      const hpp_board*            board,
                                      size_t                      row,
                                      size_t                      col,
                                      uint8_t                     new_value)
{
    if (cost == NULL || board == NULL)
    {
        return 0;
    }

    uint8_t old_value = board->cells[row][col];
    if (old_value == new_value)
    {
        return 0; // No change
    }

    // Compute old state metrics (before the change)
    uint32_t old_col_cost = cost->col_costs[col];

    size_t   block_row      = row / cost->block_size;
    size_t   block_col      = col / cost->block_size;
    uint32_t old_block_cost = cost->block_costs[block_row][block_col];

    // Simulate the change: count new state
    // Create a temporary board state to compute new costs
    hpp_board temp_board       = *board;
    temp_board.cells[row][col] = new_value;

    uint32_t new_col_cost   = count_conflicts_in_col(&temp_board, col);
    uint32_t new_block_cost = count_conflicts_in_block(&temp_board, block_row, block_col);

    // Compute delta (rows always valid, no need to check)
    int32_t delta = (int32_t)new_col_cost - (int32_t)old_col_cost + (int32_t)new_block_cost -
                    (int32_t)old_block_cost;

    return delta;
}

int32_t incremental_cost_delta_swap(const hpp_incremental_cost* cost,
                                    const hpp_board*            board,
                                    size_t                      row1,
                                    size_t                      col1,
                                    size_t                      row2,
                                    size_t                      col2)
{
    if (cost == NULL || board == NULL)
    {
        return 0;
    }

    // Save old costs (rows always valid, no need to track)
    uint32_t old_col1_cost = cost->col_costs[col1];

    size_t   block_row1      = row1 / cost->block_size;
    size_t   block_col1      = col1 / cost->block_size;
    uint32_t old_block1_cost = cost->block_costs[block_row1][block_col1];

    uint32_t old_col2_cost = cost->col_costs[col2];

    size_t   block_row2      = row2 / cost->block_size;
    size_t   block_col2      = col2 / cost->block_size;
    uint32_t old_block2_cost = cost->block_costs[block_row2][block_col2];

    // Simulate the swap
    hpp_board temp_board         = *board;
    uint8_t   temp               = temp_board.cells[row1][col1];
    temp_board.cells[row1][col1] = temp_board.cells[row2][col2];
    temp_board.cells[row2][col2] = temp;

    // Compute new costs (rows always valid, no need to check)
    uint32_t new_col1_cost   = count_conflicts_in_col(&temp_board, col1);
    uint32_t new_block1_cost = count_conflicts_in_block(&temp_board, block_row1, block_col1);

    uint32_t new_col2_cost   = count_conflicts_in_col(&temp_board, col2);
    uint32_t new_block2_cost = count_conflicts_in_block(&temp_board, block_row2, block_col2);

    // Compute delta (handle case where cols/blocks overlap)
    int32_t delta = 0;

    if (col1 != col2)
    {
        delta += (int32_t)new_col1_cost - (int32_t)old_col1_cost;
        delta += (int32_t)new_col2_cost - (int32_t)old_col2_cost;
    }
    else
    {
        // Same column: only compute once
        delta += (int32_t)new_col1_cost - (int32_t)old_col1_cost;
    }

    if (block_row1 != block_row2 || block_col1 != block_col2)
    {
        delta += (int32_t)new_block1_cost - (int32_t)old_block1_cost;
        delta += (int32_t)new_block2_cost - (int32_t)old_block2_cost;
    }
    else
    {
        // Same block: only compute once
        delta += (int32_t)new_block1_cost - (int32_t)old_block1_cost;
    }

    return delta;
}

void incremental_cost_apply_move(hpp_incremental_cost* cost,
                                 const hpp_board*      board,
                                 const hpp_move*       move)
{
    if (cost == NULL || board == NULL || move == NULL)
    {
        return;
    }

    if (move->kind == MOVE_ASSIGN)
    {
        size_t row       = move->payload.assign.row;
        size_t col       = move->payload.assign.col;
        size_t block_row = row / cost->block_size;
        size_t block_col = col / cost->block_size;

        // Update costs for affected column and block (rows always valid)
        uint32_t old_col   = cost->col_costs[col];
        uint32_t old_block = cost->block_costs[block_row][block_col];

        cost->col_costs[col] = count_conflicts_in_col(board, col);
        cost->block_costs[block_row][block_col] =
            count_conflicts_in_block(board, block_row, block_col);

        // Update total
        int32_t delta = (int32_t)cost->col_costs[col] - (int32_t)old_col +
                        (int32_t)cost->block_costs[block_row][block_col] - (int32_t)old_block;

        cost->total_violations = (uint32_t)((int32_t)cost->total_violations + delta);
    }
    else if (move->kind == MOVE_SWAP)
    {
        size_t row1 = move->payload.swap.row1;
        size_t col1 = move->payload.swap.col1;
        size_t row2 = move->payload.swap.row2;
        size_t col2 = move->payload.swap.col2;

        size_t block_row1 = row1 / cost->block_size;
        size_t block_col1 = col1 / cost->block_size;
        size_t block_row2 = row2 / cost->block_size;
        size_t block_col2 = col2 / cost->block_size;

        int32_t delta = 0;

        // Update column costs (rows always valid, no need to track)
        if (col1 != col2)
        {
            uint32_t old_col1     = cost->col_costs[col1];
            uint32_t old_col2     = cost->col_costs[col2];
            cost->col_costs[col1] = count_conflicts_in_col(board, col1);
            cost->col_costs[col2] = count_conflicts_in_col(board, col2);
            delta += (int32_t)cost->col_costs[col1] - (int32_t)old_col1;
            delta += (int32_t)cost->col_costs[col2] - (int32_t)old_col2;
        }
        else
        {
            uint32_t old_col      = cost->col_costs[col1];
            cost->col_costs[col1] = count_conflicts_in_col(board, col1);
            delta += (int32_t)cost->col_costs[col1] - (int32_t)old_col;
        }

        // Update block costs
        if (block_row1 != block_row2 || block_col1 != block_col2)
        {
            uint32_t old_block1 = cost->block_costs[block_row1][block_col1];
            uint32_t old_block2 = cost->block_costs[block_row2][block_col2];
            cost->block_costs[block_row1][block_col1] =
                count_conflicts_in_block(board, block_row1, block_col1);
            cost->block_costs[block_row2][block_col2] =
                count_conflicts_in_block(board, block_row2, block_col2);
            delta += (int32_t)cost->block_costs[block_row1][block_col1] - (int32_t)old_block1;
            delta += (int32_t)cost->block_costs[block_row2][block_col2] - (int32_t)old_block2;
        }
        else
        {
            uint32_t old_block = cost->block_costs[block_row1][block_col1];
            cost->block_costs[block_row1][block_col1] =
                count_conflicts_in_block(board, block_row1, block_col1);
            delta += (int32_t)cost->block_costs[block_row1][block_col1] - (int32_t)old_block;
        }

        cost->total_violations = (uint32_t)((int32_t)cost->total_violations + delta);
    }
}
