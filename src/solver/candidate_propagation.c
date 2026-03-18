/**
 * @file solver/candidate_propagation.c
 * @brief Constraint-propagation and candidate-domain maintenance.
 */

#include "solver/candidate_internal.h"

static bool
hpp_candidate_row_has_singleton(const hpp_candidate_state* state, size_t row_index, size_t value)
{
    const hpp_bitvector_word* row_bits = hpp_candidate_row_bits(state->constraints, row_index);
    return hpp_candidate_bit_test(row_bits, value);
}

static bool
hpp_candidate_col_has_singleton(const hpp_candidate_state* state, size_t col_index, size_t value)
{
    const hpp_bitvector_word* col_bits = hpp_candidate_col_bits(state->constraints, col_index);
    return hpp_candidate_bit_test(col_bits, value);
}

static bool hpp_candidate_box_has_singleton(const hpp_candidate_state* state,
                                            size_t                     box_row,
                                            size_t                     box_col,
                                            size_t                     value)
{
    const size_t              box_index = (box_row * state->board->block_length) + box_col;
    const hpp_bitvector_word* box_bits  = hpp_candidate_box_bits(state->constraints, box_index);
    return hpp_candidate_bit_test(box_bits, value);
}

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

    hpp_candidate_bit_clear(candidates, value);
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
