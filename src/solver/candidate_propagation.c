/**
 * @file solver/candidate_propagation.c
 * @brief Constraint-propagation and candidate-domain maintenance.
 */

#include "solver/candidate_internal.h"

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static bool
hpp_candidate_row_has_singleton(const hpp_candidate_state* state, size_t row_index, size_t value);
static bool
hpp_candidate_col_has_singleton(const hpp_candidate_state* state, size_t col_index, size_t value);
static bool hpp_candidate_box_has_singleton(const hpp_candidate_state* state,
                                            size_t                     box_row,
                                            size_t                     box_col,
                                            size_t                     value);
static bool hpp_candidate_find_hidden_single_in_row(const hpp_candidate_state* state,
                                                    size_t                     row,
                                                    size_t*                    cell_index,
                                                    size_t*                    value);
static bool hpp_candidate_find_hidden_single_in_col(const hpp_candidate_state* state,
                                                    size_t                     col,
                                                    size_t*                    cell_index,
                                                    size_t*                    value);
static bool hpp_candidate_find_hidden_single_in_box(const hpp_candidate_state* state,
                                                    size_t                     box_row,
                                                    size_t                     box_col,
                                                    size_t*                    cell_index,
                                                    size_t*                    value);
static bool hpp_candidate_find_hidden_single_around_cell(const hpp_candidate_state* state,
                                                         size_t                     cell_index,
                                                         size_t*                    hidden_cell,
                                                         size_t*                    hidden_value);
static inline const size_t* hpp_candidate_row_cells(const hpp_candidate_state* state, size_t row);
static inline const size_t* hpp_candidate_col_cells(const hpp_candidate_state* state, size_t col);
static inline const size_t* hpp_candidate_box_cells(const hpp_candidate_state* state,
                                                    size_t                     box_idx);
static bool
hpp_candidate_remove_value_from_cell(hpp_candidate_state* state, size_t cell_index, size_t value);
static bool hpp_candidate_apply_assignment_to_peers(hpp_candidate_state* state,
                                                    size_t               cell_index,
                                                    size_t               value);

/* =========================================================================
 * Public API
 * ========================================================================= */

bool hpp_candidate_state_propagate_singles(hpp_candidate_state* state)
{
    size_t modified_cell_index = SIZE_MAX;
    while (hpp_candidate_modified_pop(state, &modified_cell_index))
    {
        if (state->board->cells[modified_cell_index] != BOARD_CELL_EMPTY)
        {
            const size_t assigned_value = (size_t)state->board->cells[modified_cell_index];
            if (!hpp_candidate_apply_assignment_to_peers(
                    state, modified_cell_index, assigned_value))
            {
                return false;
            }
        }

        if (state->board->cells[modified_cell_index] == BOARD_CELL_EMPTY)
        {
            const size_t candidate_count = state->candidate_counts[modified_cell_index];
            if (candidate_count == 0)
            {
                return false;
            }

            if (candidate_count == 1)
            {
                const size_t single_value =
                    hpp_candidate_ac3_singleton_value_for_cell(state, modified_cell_index);
                if (single_value == BOARD_CELL_EMPTY ||
                    !hpp_candidate_state_assign(state, modified_cell_index, single_value))
                {
                    return false;
                }

                continue;
            }
        }

        size_t hidden_single_cell  = SIZE_MAX;
        size_t hidden_single_value = BOARD_CELL_EMPTY;
        if (hpp_candidate_find_hidden_single_around_cell(
                state, modified_cell_index, &hidden_single_cell, &hidden_single_value))
        {
            if (!hpp_candidate_state_assign(state, hidden_single_cell, hidden_single_value))
            {
                return false;
            }
        }
    }

    return true;
}

/* =========================================================================
 * Internal Helpers
 * ========================================================================= */

/**
 * @brief Return pointer to row traversal-order slice.
 *
 * @note Row slices are contiguous with `side_length` entries per row.
 * @pre `state != NULL` and `row < state->board->side_length`.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state owning traversal caches.
 * @param row Row index to address.
 * @return Const pointer to first row-ordered cell index for `row`.
 */
static inline const size_t* hpp_candidate_row_cells(const hpp_candidate_state* state, size_t row)
{
    return state->row_cell_order + (row * state->board->side_length);
}

/**
 * @brief Return pointer to column traversal-order slice.
 *
 * @note Column slices are contiguous with `side_length` entries per column.
 * @pre `state != NULL` and `col < state->board->side_length`.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state owning traversal caches.
 * @param col Column index to address.
 * @return Const pointer to first column-ordered cell index for `col`.
 */
static inline const size_t* hpp_candidate_col_cells(const hpp_candidate_state* state, size_t col)
{
    return state->col_cell_order + (col * state->board->side_length);
}

/**
 * @brief Return pointer to box traversal-order slice.
 *
 * @note Box slices are contiguous with `side_length` entries per box.
 * @pre `state != NULL` and `box_idx` is valid for board geometry.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state owning traversal caches.
 * @param box_idx Box index to address.
 * @return Const pointer to first box-ordered cell index for `box_idx`.
 */
static inline const size_t* hpp_candidate_box_cells(const hpp_candidate_state* state,
                                                    size_t                     box_idx)
{
    return state->box_cell_order + (box_idx * state->board->side_length);
}

/**
 * @brief Check whether a row already has a singleton assignment for a value.
 *
 * @note Reads cached row occupancy bits from validation constraints rather
 *       than scanning row cells.
 * @pre `state != NULL` and `row_index` is in range.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state with constraint cache.
 * @param row_index Row index to query.
 * @param value Candidate value to test.
 * @return `true` if the row already contains `value` as a fixed assignment.
 */
static bool
hpp_candidate_row_has_singleton(const hpp_candidate_state* state, size_t row_index, size_t value)
{
    const hpp_bitvector_word* row_bits = hpp_candidate_row_bits(state->constraints, row_index);
    return hpp_candidate_bit_test(row_bits, value);
}

/**
 * @brief Check whether a column already has a singleton assignment for a value.
 *
 * @note Reads cached column occupancy bits from validation constraints.
 * @pre `state != NULL` and `col_index` is in range.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state with constraint cache.
 * @param col_index Column index to query.
 * @param value Candidate value to test.
 * @return `true` if the column already contains `value` as fixed assignment.
 */
static bool
hpp_candidate_col_has_singleton(const hpp_candidate_state* state, size_t col_index, size_t value)
{
    const hpp_bitvector_word* col_bits = hpp_candidate_col_bits(state->constraints, col_index);
    return hpp_candidate_bit_test(col_bits, value);
}

/**
 * @brief Check whether a box already has a singleton assignment for a value.
 *
 * @note Computes linear box index from `(box_row, box_col)` in box-grid space.
 * @pre `state != NULL` and box coordinates are in range.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state with constraint cache.
 * @param box_row Box-grid row index.
 * @param box_col Box-grid column index.
 * @param value Candidate value to test.
 * @return `true` if the box already contains `value` as fixed assignment.
 */
static bool hpp_candidate_box_has_singleton(const hpp_candidate_state* state,
                                            size_t                     box_row,
                                            size_t                     box_col,
                                            size_t                     value)
{
    const size_t              box_index = (box_row * state->board->block_length) + box_col;
    const hpp_bitvector_word* box_bits  = hpp_candidate_box_bits(state->constraints, box_index);
    return hpp_candidate_bit_test(box_bits, value);
}

/**
 * @brief Search one row for hidden singles.
 *
 * @note A hidden single exists when exactly one unassigned cell in the row
 *       still admits a value not yet fixed in that row.
 * @pre `state`, `cell_index`, and `value` are non-NULL; `row` is valid.
 * @post On success, outputs receive inferred assignment location/value.
 *
 * @param state Candidate state to inspect.
 * @param row Row index to scan.
 * @param cell_index Output inferred cell index.
 * @param value Output inferred value.
 * @return `true` when a hidden single is found in the row.
 */
static bool hpp_candidate_find_hidden_single_in_row(const hpp_candidate_state* state,
                                                    size_t                     row,
                                                    size_t*                    cell_index,
                                                    size_t*                    value)
{
    const size_t side_length = state->board->side_length;

    for (size_t candidate_value = 1; candidate_value <= side_length; ++candidate_value)
    {
        if (hpp_candidate_row_has_singleton(state, row, candidate_value))
        {
            continue;
        }

        size_t        candidate_cell  = SIZE_MAX;
        size_t        candidate_count = 0;
        const size_t* row_cells       = hpp_candidate_row_cells(state, row);
        for (size_t offset = 0; offset < side_length; ++offset)
        {
            const size_t idx = row_cells[offset];
            if (state->board->cells[idx] != BOARD_CELL_EMPTY)
            {
                continue;
            }

            if (!hpp_candidate_bit_test(hpp_candidate_cell_at_const(state, idx), candidate_value))
            {
                continue;
            }

            candidate_cell = idx;
            candidate_count++;
            if (candidate_count > 1)
            {
                break;
            }
        }

        if (candidate_count == 1)
        {
            *cell_index = candidate_cell;
            *value      = candidate_value;
            return true;
        }
    }

    return false;
}

/**
 * @brief Search one column for hidden singles.
 *
 * @note Mirrors row logic but traverses column-local cell order cache.
 * @pre `state`, `cell_index`, and `value` are non-NULL; `col` is valid.
 * @post On success, outputs receive inferred assignment location/value.
 *
 * @param state Candidate state to inspect.
 * @param col Column index to scan.
 * @param cell_index Output inferred cell index.
 * @param value Output inferred value.
 * @return `true` when a hidden single is found in the column.
 */
static bool hpp_candidate_find_hidden_single_in_col(const hpp_candidate_state* state,
                                                    size_t                     col,
                                                    size_t*                    cell_index,
                                                    size_t*                    value)
{
    const size_t side_length = state->board->side_length;

    for (size_t candidate_value = 1; candidate_value <= side_length; ++candidate_value)
    {
        if (hpp_candidate_col_has_singleton(state, col, candidate_value))
        {
            continue;
        }

        size_t        candidate_cell  = SIZE_MAX;
        size_t        candidate_count = 0;
        const size_t* col_cells       = hpp_candidate_col_cells(state, col);
        for (size_t offset = 0; offset < side_length; ++offset)
        {
            const size_t idx = col_cells[offset];
            if (state->board->cells[idx] != BOARD_CELL_EMPTY)
            {
                continue;
            }

            if (!hpp_candidate_bit_test(hpp_candidate_cell_at_const(state, idx), candidate_value))
            {
                continue;
            }

            candidate_cell = idx;
            candidate_count++;
            if (candidate_count > 1)
            {
                break;
            }
        }

        if (candidate_count == 1)
        {
            *cell_index = candidate_cell;
            *value      = candidate_value;
            return true;
        }
    }

    return false;
}

/**
 * @brief Search one box for hidden singles.
 *
 * @note Scans box-local traversal order and ignores already-assigned cells.
 * @pre `state`, `cell_index`, and `value` are non-NULL; box coords are valid.
 * @post On success, outputs receive inferred assignment location/value.
 *
 * @param state Candidate state to inspect.
 * @param box_row Box-grid row index.
 * @param box_col Box-grid column index.
 * @param cell_index Output inferred cell index.
 * @param value Output inferred value.
 * @return `true` when a hidden single is found in the box.
 */
static bool hpp_candidate_find_hidden_single_in_box(const hpp_candidate_state* state,
                                                    size_t                     box_row,
                                                    size_t                     box_col,
                                                    size_t*                    cell_index,
                                                    size_t*                    value)
{
    const size_t side_length = state->board->side_length;
    const size_t box_index   = (box_row * state->board->block_length) + box_col;

    for (size_t candidate_value = 1; candidate_value <= side_length; ++candidate_value)
    {
        if (hpp_candidate_box_has_singleton(state, box_row, box_col, candidate_value))
        {
            continue;
        }

        size_t        candidate_cell  = SIZE_MAX;
        size_t        candidate_count = 0;
        const size_t* box_cells       = hpp_candidate_box_cells(state, box_index);
        for (size_t offset = 0; offset < side_length; ++offset)
        {
            const size_t idx = box_cells[offset];
            if (state->board->cells[idx] != BOARD_CELL_EMPTY)
            {
                continue;
            }

            if (!hpp_candidate_bit_test(hpp_candidate_cell_at_const(state, idx), candidate_value))
            {
                continue;
            }

            candidate_cell = idx;
            candidate_count++;
            if (candidate_count > 1)
            {
                break;
            }
        }

        if (candidate_count == 1)
        {
            *cell_index = candidate_cell;
            *value      = candidate_value;
            return true;
        }
    }

    return false;
}

/**
 * @brief Search row, column, and box around a cell for a hidden single.
 *
 * @note Used as a localized inference pass after a cell changes, reducing
 *       unnecessary global scans during propagation.
 * @pre `state`, `hidden_cell`, and `hidden_value` are non-NULL.
 * @post On success, outputs contain inferred assignment.
 *
 * @param state Candidate state to inspect.
 * @param cell_index Anchor cell whose neighborhood is scanned.
 * @param hidden_cell Output inferred cell index.
 * @param hidden_value Output inferred value.
 * @return `true` if any unit around `cell_index` yields a hidden single.
 */
static bool hpp_candidate_find_hidden_single_around_cell(const hpp_candidate_state* state,
                                                         size_t                     cell_index,
                                                         size_t*                    hidden_cell,
                                                         size_t*                    hidden_value)
{
    const size_t side_length = state->board->side_length;
    const size_t row_index   = cell_index / side_length;
    const size_t col_index   = cell_index % side_length;
    const size_t box_row     = row_index / state->board->block_length;
    const size_t box_col     = col_index / state->board->block_length;

    return hpp_candidate_find_hidden_single_in_row(state, row_index, hidden_cell, hidden_value) ||
           hpp_candidate_find_hidden_single_in_col(state, col_index, hidden_cell, hidden_value) ||
           hpp_candidate_find_hidden_single_in_box(
               state, box_row, box_col, hidden_cell, hidden_value);
}

/**
 * @brief Remove one candidate value from a cell and enqueue if changed.
 *
 * @note This function enforces contradiction detection when a domain becomes
 *       empty (`candidate_counts[cell] == 0`).
 * @pre `state != NULL` and `cell_index` is valid.
 * @post Candidate domain/count for `cell_index` may shrink and be enqueued.
 *
 * @param state Candidate state to mutate.
 * @param cell_index Cell whose domain may be pruned.
 * @param value Candidate value to remove.
 * @return `false` when pruning creates a contradiction or queue push fails.
 */
static bool
hpp_candidate_remove_value_from_cell(hpp_candidate_state* state, size_t cell_index, size_t value)
{
    if (state->board->cells[cell_index] != BOARD_CELL_EMPTY)
    {
        return true;
    }

    hpp_bitvector_word* candidates = hpp_candidate_cell_at(state, cell_index);
    if (!hpp_candidate_bit_test(candidates, value))
    {
        return true;
    }

    hpp_bitvector_clear(candidates, value);
    if (state->candidate_counts[cell_index] == 0)
    {
        return false;
    }

    state->candidate_counts[cell_index]--;
    if (state->candidate_counts[cell_index] == 0)
    {
        return false;
    }

    return hpp_candidate_modified_push(state, cell_index);
}

/**
 * @brief Propagate an assignment by removing its value from all peers.
 *
 * @note Iterates row, column, and box peers using cached locality orders.
 *       Box iteration skips peers already covered by row/column traversal.
 * @pre `state != NULL` and `cell_index` addresses an assigned cell.
 * @post Peer domains may be reduced; contradiction returns `false`.
 *
 * @param state Candidate state to mutate.
 * @param cell_index Newly assigned anchor cell.
 * @param value Assigned value to eliminate from peers.
 * @return `true` if peer pruning completes without contradiction.
 */
static bool
hpp_candidate_apply_assignment_to_peers(hpp_candidate_state* state, size_t cell_index, size_t value)
{
    const size_t side_length  = state->board->side_length;
    const size_t block_length = state->board->block_length;

    const size_t row_index = cell_index / side_length;
    const size_t col_index = cell_index % side_length;
    const size_t box_row   = row_index / block_length;
    const size_t box_col   = col_index / block_length;
    const size_t box_index = (box_row * block_length) + box_col;

    const size_t* row_cells = hpp_candidate_row_cells(state, row_index);
    for (size_t offset = 0; offset < side_length; ++offset)
    {
        const size_t peer_index = row_cells[offset];
        if (peer_index == cell_index)
        {
            continue;
        }

        if (!hpp_candidate_remove_value_from_cell(state, peer_index, value))
        {
            return false;
        }
    }

    const size_t* col_cells = hpp_candidate_col_cells(state, col_index);
    for (size_t offset = 0; offset < side_length; ++offset)
    {
        const size_t peer_index = col_cells[offset];
        if (peer_index == cell_index)
        {
            continue;
        }

        if (!hpp_candidate_remove_value_from_cell(state, peer_index, value))
        {
            return false;
        }
    }

    const size_t* box_cells = hpp_candidate_box_cells(state, box_index);
    for (size_t offset = 0; offset < side_length; ++offset)
    {
        const size_t peer_index = box_cells[offset];
        if (peer_index == cell_index)
        {
            continue;
        }

        const size_t peer_row = peer_index / side_length;
        const size_t peer_col = peer_index % side_length;
        if (peer_row == row_index || peer_col == col_index)
        {
            continue;
        }

        if (!hpp_candidate_remove_value_from_cell(state, peer_index, value))
        {
            return false;
        }
    }

    return true;
}
