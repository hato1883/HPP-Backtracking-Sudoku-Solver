/**
 * @file solver/candidate.c
 * @brief Candidate-state lifecycle, cloning, and shared helpers.
 */

#include "solver/candidate_internal.h"

#include <stdlib.h>

bool hpp_candidate_validate_initial_board(const hpp_board*            board,
                                          hpp_validation_constraints* constraints)
{
    if (board == NULL || board->cells == NULL || board->side_length == 0 ||
        board->block_length == 0 || board->cell_count != board->side_length * board->side_length)
    {
        return false;
    }

    if (board->block_length * board->block_length != board->side_length ||
        board->side_length > UINT8_MAX)
    {
        return false;
    }

    return hpp_validation_constraints_init_from_board(constraints, board);
}

hpp_validation_constraints*
hpp_candidate_constraints_clone(const hpp_validation_constraints* source)
{
    if (source == NULL)
    {
        return NULL;
    }

    hpp_validation_constraints* clone =
        hpp_validation_constraints_create(source->side_length, source->block_length);
    if (clone == NULL)
    {
        return NULL;
    }

    const size_t side_bytes     = source->side_length * HPP_CANDIDATE_CELL_BYTES;
    const size_t boxes_per_side = source->side_length / source->block_length;
    const size_t box_count      = boxes_per_side * boxes_per_side;
    const size_t box_bytes      = box_count * HPP_CANDIDATE_CELL_BYTES;

    memcpy(clone->row_bitvectors, source->row_bitvectors, side_bytes);
    memcpy(clone->col_bitvectors, source->col_bitvectors, side_bytes);
    memcpy(clone->box_bitvectors, source->box_bitvectors, box_bytes);

    return clone;
}

void hpp_candidate_constraints_copy(hpp_validation_constraints*       destination,
                                    const hpp_validation_constraints* source)
{
    const size_t side_bytes     = source->side_length * HPP_CANDIDATE_CELL_BYTES;
    const size_t boxes_per_side = source->side_length / source->block_length;
    const size_t box_count      = boxes_per_side * boxes_per_side;
    const size_t box_bytes      = box_count * HPP_CANDIDATE_CELL_BYTES;

    memcpy(destination->row_bitvectors, source->row_bitvectors, side_bytes);
    memcpy(destination->col_bitvectors, source->col_bitvectors, side_bytes);
    memcpy(destination->box_bitvectors, source->box_bitvectors, box_bytes);
}

bool hpp_candidate_build_spatial_orders(hpp_candidate_state* state)
{
    const size_t side_length  = state->board->side_length;
    const size_t cell_count   = state->board->cell_count;
    const size_t block_length = state->board->block_length;

    state->row_cell_order = malloc(cell_count * sizeof(size_t));
    state->col_cell_order = malloc(cell_count * sizeof(size_t));
    state->box_cell_order = malloc(cell_count * sizeof(size_t));
    if (state->row_cell_order == NULL || state->col_cell_order == NULL ||
        state->box_cell_order == NULL)
    {
        hpp_candidate_destroy_spatial_orders(state);
        return false;
    }

    size_t write_idx = 0;
    // Row order: r1c1..r1cN, r2c1..r2cN, ...
    for (size_t row = 0; row < side_length; ++row)
    {
        for (size_t col = 0; col < side_length; ++col)
        {
            state->row_cell_order[write_idx++] = (row * side_length) + col;
        }
    }

    write_idx = 0;
    // Column order: r1c1..rNc1, r1c2..rNc2, ...
    for (size_t col = 0; col < side_length; ++col)
    {
        for (size_t row = 0; row < side_length; ++row)
        {
            state->col_cell_order[write_idx++] = (row * side_length) + col;
        }
    }

    write_idx = 0;
    // Box order: top-left to bottom-right inside each box, then next box.
    for (size_t box_row = 0; box_row < block_length; ++box_row)
    {
        for (size_t box_col = 0; box_col < block_length; ++box_col)
        {
            const size_t row_start = box_row * block_length;
            const size_t col_start = box_col * block_length;

            for (size_t row_offset = 0; row_offset < block_length; ++row_offset)
            {
                for (size_t col_offset = 0; col_offset < block_length; ++col_offset)
                {
                    const size_t row                   = row_start + row_offset;
                    const size_t col                   = col_start + col_offset;
                    state->box_cell_order[write_idx++] = (row * side_length) + col;
                }
            }
        }
    }

    return true;
}

bool hpp_candidate_clone_spatial_orders(const hpp_candidate_state* source,
                                        hpp_candidate_state*       destination)
{
    const size_t cell_bytes = source->board->cell_count * sizeof(size_t);

    destination->row_cell_order = malloc(cell_bytes);
    destination->col_cell_order = malloc(cell_bytes);
    destination->box_cell_order = malloc(cell_bytes);
    if (destination->row_cell_order == NULL || destination->col_cell_order == NULL ||
        destination->box_cell_order == NULL)
    {
        hpp_candidate_destroy_spatial_orders(destination);
        return false;
    }

    memcpy(destination->row_cell_order, source->row_cell_order, cell_bytes);
    memcpy(destination->col_cell_order, source->col_cell_order, cell_bytes);
    memcpy(destination->box_cell_order, source->box_cell_order, cell_bytes);
    return true;
}

void hpp_candidate_destroy_spatial_orders(hpp_candidate_state* state)
{
    free(state->row_cell_order);
    free(state->col_cell_order);
    free(state->box_cell_order);

    state->row_cell_order = NULL;
    state->col_cell_order = NULL;
    state->box_cell_order = NULL;
}

static size_t hpp_candidate_count_unassigned_cells(const hpp_board* board)
{
    size_t unassigned_count = 0;

    for (size_t index = 0; index < board->cell_count; ++index)
    {
        if (board->cells[index] == BOARD_CELL_EMPTY)
        {
            unassigned_count++;
        }
    }

    return unassigned_count;
}

bool hpp_candidate_modified_push(hpp_candidate_state* state, size_t cell_index)
{
    if (state->modified_cells == NULL || state->modified_in_stack == NULL ||
        cell_index >= state->board->cell_count)
    {
        return false;
    }

    if (state->modified_in_stack[cell_index] != 0U)
    {
        return true;
    }

    if (state->modified_count >= state->board->cell_count)
    {
        return false;
    }

    state->modified_cells[state->modified_count++] = cell_index;
    state->modified_in_stack[cell_index]           = 1U;
    return true;
}

bool hpp_candidate_modified_pop(hpp_candidate_state* state, size_t* cell_index)
{
    if (state->modified_count == 0)
    {
        return false;
    }

    state->modified_count--;
    *cell_index                           = state->modified_cells[state->modified_count];
    state->modified_in_stack[*cell_index] = 0U;
    return true;
}

void hpp_candidate_modified_reset(hpp_candidate_state* state)
{
    state->modified_count = 0;
    memset(state->modified_in_stack, 0, state->board->cell_count * sizeof(uint8_t));
}

size_t hpp_candidate_find_single_value_for_cell(const hpp_candidate_state* state, size_t cell_index)
{
    const hpp_bitvector_word* candidates = hpp_candidate_cell_at_const(state, cell_index);
    for (size_t value = 1; value <= state->board->side_length; ++value)
    {
        if (hpp_candidate_bit_test(candidates, value))
        {
            return value;
        }
    }

    return BOARD_CELL_EMPTY;
}

size_t hpp_candidate_ac3_singleton_value_for_cell(const hpp_candidate_state* state,
                                                  size_t                     cell_index)
{
    const hpp_cell assigned_value = state->board->cells[cell_index];
    if (assigned_value != BOARD_CELL_EMPTY)
    {
        return (size_t)assigned_value;
    }

    if (state->candidate_counts[cell_index] != 1)
    {
        return BOARD_CELL_EMPTY;
    }

    return hpp_candidate_find_single_value_for_cell(state, cell_index);
}

size_t
hpp_candidate_compute_cell(hpp_candidate_state* state, size_t cell_index, size_t* single_value)
{
    hpp_bitvector_word* cell_candidates = hpp_candidate_cell_at(state, cell_index);
    memset(cell_candidates, 0, HPP_CANDIDATE_CELL_BYTES);

    if (state->board->cells[cell_index] != BOARD_CELL_EMPTY)
    {
        state->candidate_counts[cell_index] = 0;
        if (single_value != NULL)
        {
            *single_value = BOARD_CELL_EMPTY;
        }
        return 0;
    }

    const size_t              row     = cell_index / state->board->side_length;
    const size_t              col     = cell_index % state->board->side_length;
    const size_t              box_idx = hpp_candidate_box_index(state->board, row, col);
    const hpp_bitvector_word* row_bv  = hpp_candidate_row_bits(state->constraints, row);
    const hpp_bitvector_word* col_bv  = hpp_candidate_col_bits(state->constraints, col);
    const hpp_bitvector_word* box_bv  = hpp_candidate_box_bits(state->constraints, box_idx);

    size_t count      = 0;
    size_t last_value = BOARD_CELL_EMPTY;
    for (size_t value = 1; value <= state->board->side_length; ++value)
    {
        if (hpp_candidate_bit_test(row_bv, value) || hpp_candidate_bit_test(col_bv, value) ||
            hpp_candidate_bit_test(box_bv, value))
        {
            continue;
        }

        hpp_candidate_bit_set(cell_candidates, value);
        last_value = value;
        count++;
    }

    state->candidate_counts[cell_index] = count;
    if (single_value != NULL)
    {
        *single_value = (count == 1) ? last_value : BOARD_CELL_EMPTY;
    }

    return count;
}

bool hpp_candidate_initialize_cache_and_work_stack(hpp_candidate_state* state)
{
    hpp_candidate_modified_reset(state);

    for (size_t idx = 0; idx < state->board->cell_count; ++idx)
    {
        if (state->board->cells[idx] == BOARD_CELL_EMPTY)
        {
            (void)hpp_candidate_compute_cell(state, idx, NULL);
        }
        else
        {
            memset(hpp_candidate_cell_at(state, idx), 0, HPP_CANDIDATE_CELL_BYTES);
            state->candidate_counts[idx] = 0;
        }

        if (!hpp_candidate_modified_push(state, idx))
        {
            return false;
        }
    }

    return true;
}

hpp_candidate_init_status hpp_candidate_state_init_from_board(hpp_candidate_state* state,
                                                              const hpp_board*     source)
{
    memset(state, 0, sizeof(*state));

    state->board = hpp_board_clone(source);
    if (state->board == NULL)
    {
        return HPP_CANDIDATE_INIT_ERROR;
    }

    state->constraints =
        hpp_validation_constraints_create(state->board->side_length, state->board->block_length);
    if (state->constraints == NULL)
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    if (!hpp_candidate_validate_initial_board(state->board, state->constraints))
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_INVALID_BOARD;
    }

    state->candidates =
        calloc(state->board->cell_count * HPP_CANDIDATE_WORD_COUNT, sizeof(hpp_bitvector_word));
    if (state->candidates == NULL)
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    state->candidate_counts = calloc(state->board->cell_count, sizeof(size_t));
    if (state->candidate_counts == NULL)
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    if (!hpp_candidate_build_spatial_orders(state))
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    state->modified_cells = malloc(state->board->cell_count * sizeof(size_t));
    if (state->modified_cells == NULL)
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    state->modified_in_stack = calloc(state->board->cell_count, sizeof(uint8_t));
    if (state->modified_in_stack == NULL)
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    state->remaining_unassigned = hpp_candidate_count_unassigned_cells(state->board);
    if (!hpp_candidate_initialize_cache_and_work_stack(state))
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    return HPP_CANDIDATE_INIT_OK;
}

void hpp_candidate_state_destroy(hpp_candidate_state* state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->board != NULL)
    {
        destroy_board(&state->board);
    }

    if (state->constraints != NULL)
    {
        hpp_validation_constraints_destroy(&state->constraints);
    }

    free(state->candidates);
    free(state->candidate_counts);
    hpp_candidate_destroy_spatial_orders(state);
    free(state->modified_cells);
    free(state->modified_in_stack);

    state->candidates           = NULL;
    state->candidate_counts     = NULL;
    state->modified_cells       = NULL;
    state->modified_in_stack    = NULL;
    state->modified_count       = 0;
    state->remaining_unassigned = 0;
}

bool hpp_candidate_state_clone(const hpp_candidate_state* source, hpp_candidate_state* destination)
{
    memset(destination, 0, sizeof(*destination));

    destination->board = hpp_board_clone(source->board);
    if (destination->board == NULL)
    {
        return false;
    }

    destination->constraints = hpp_candidate_constraints_clone(source->constraints);
    if (destination->constraints == NULL)
    {
        hpp_candidate_state_destroy(destination);
        return false;
    }

    const size_t candidate_buffer_size = hpp_candidate_buffer_size(source->board);
    destination->candidates            = malloc(candidate_buffer_size);
    if (destination->candidates == NULL)
    {
        hpp_candidate_state_destroy(destination);
        return false;
    }

    destination->candidate_counts = malloc(source->board->cell_count * sizeof(size_t));
    if (destination->candidate_counts == NULL)
    {
        hpp_candidate_state_destroy(destination);
        return false;
    }

    if (!hpp_candidate_clone_spatial_orders(source, destination))
    {
        hpp_candidate_state_destroy(destination);
        return false;
    }

    destination->modified_cells = malloc(source->board->cell_count * sizeof(size_t));
    if (destination->modified_cells == NULL)
    {
        hpp_candidate_state_destroy(destination);
        return false;
    }

    destination->modified_in_stack = malloc(source->board->cell_count * sizeof(uint8_t));
    if (destination->modified_in_stack == NULL)
    {
        hpp_candidate_state_destroy(destination);
        return false;
    }

    memcpy(destination->candidates, source->candidates, candidate_buffer_size);
    memcpy(destination->candidate_counts,
           source->candidate_counts,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->modified_cells,
           source->modified_cells,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->modified_in_stack,
           source->modified_in_stack,
           source->board->cell_count * sizeof(uint8_t));

    destination->modified_count       = source->modified_count;
    destination->remaining_unassigned = source->remaining_unassigned;
    return true;
}

bool hpp_candidate_state_assign(hpp_candidate_state* state, size_t cell_index, size_t value)
{
    if (value == BOARD_CELL_EMPTY || value > state->board->side_length)
    {
        return false;
    }

    if (state->board->cells[cell_index] != BOARD_CELL_EMPTY)
    {
        return false;
    }

    const size_t row = cell_index / state->board->side_length;
    const size_t col = cell_index % state->board->side_length;

    if (!hpp_validation_can_place_value(state->constraints, row, col, value))
    {
        return false;
    }

    state->board->cells[cell_index] = (hpp_cell)value;

    const bool row_added = hpp_validation_row_add_value(state->constraints, row, value);
    const bool col_added =
        row_added && hpp_validation_col_add_value(state->constraints, col, value);
    const bool box_added =
        col_added && hpp_validation_box_add_value(state->constraints, row, col, value);
    if (!box_added)
    {
        if (col_added)
        {
            hpp_validation_col_remove_value(state->constraints, col, value);
        }
        if (row_added)
        {
            hpp_validation_row_remove_value(state->constraints, row, value);
        }
        state->board->cells[cell_index] = BOARD_CELL_EMPTY;
        return false;
    }

    memset(hpp_candidate_cell_at(state, cell_index), 0, HPP_CANDIDATE_CELL_BYTES);
    state->candidate_counts[cell_index] = 0;

    if (!hpp_candidate_modified_push(state, cell_index))
    {
        hpp_validation_box_remove_value(state->constraints, row, col, value);
        hpp_validation_col_remove_value(state->constraints, col, value);
        hpp_validation_row_remove_value(state->constraints, row, value);
        state->board->cells[cell_index] = BOARD_CELL_EMPTY;
        (void)hpp_candidate_compute_cell(state, cell_index, NULL);
        return false;
    }

    if (state->remaining_unassigned > 0)
    {
        state->remaining_unassigned--;
    }

    return true;
}

void hpp_candidate_state_copy_solution(hpp_candidate_state*       destination,
                                       const hpp_candidate_state* source)
{
    hpp_board_copy(destination->board, source->board);
    hpp_candidate_constraints_copy(destination->constraints, source->constraints);
    memcpy(destination->candidates, source->candidates, hpp_candidate_buffer_size(source->board));
    memcpy(destination->candidate_counts,
           source->candidate_counts,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->row_cell_order,
           source->row_cell_order,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->col_cell_order,
           source->col_cell_order,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->box_cell_order,
           source->box_cell_order,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->modified_cells,
           source->modified_cells,
           source->board->cell_count * sizeof(size_t));
    memcpy(destination->modified_in_stack,
           source->modified_in_stack,
           source->board->cell_count * sizeof(uint8_t));

    destination->modified_count       = source->modified_count;
    destination->remaining_unassigned = source->remaining_unassigned;
}
