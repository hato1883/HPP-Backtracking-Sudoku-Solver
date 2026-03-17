#include "solver/candidate.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum
{
    HPP_CANDIDATE_VALUE_COUNT = UINT8_MAX + 1U,
    HPP_CANDIDATE_BYTE_COUNT  = HPP_CANDIDATE_VALUE_COUNT / CHAR_BIT,
};

static inline void hpp_candidate_bit_position(size_t value, size_t* byte_idx, uint8_t* bit_offset)
{
    *byte_idx   = value / CHAR_BIT;
    *bit_offset = (uint8_t)(value % CHAR_BIT);
}

static inline void hpp_candidate_bit_set(uint8_t* bitvector, size_t value)
{
    size_t  byte_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_candidate_bit_position(value, &byte_idx, &bit_offset);
    bitvector[byte_idx] |= (uint8_t)(1U << bit_offset);
}

static inline bool hpp_candidate_bit_test(const uint8_t* bitvector, size_t value)
{
    size_t  byte_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_candidate_bit_position(value, &byte_idx, &bit_offset);
    return (bitvector[byte_idx] & (uint8_t)(1U << bit_offset)) != 0;
}

static inline uint8_t* hpp_candidate_cell_at(hpp_candidate_state* state, size_t cell_index)
{
    return state->candidates + (cell_index * HPP_CANDIDATE_BYTE_COUNT);
}

static inline const uint8_t* hpp_candidate_cell_at_const(const hpp_candidate_state* state,
                                                         size_t                     cell_index)
{
    return state->candidates + (cell_index * HPP_CANDIDATE_BYTE_COUNT);
}

static inline size_t hpp_candidate_buffer_size(const hpp_board* board)
{
    return board->cell_count * HPP_CANDIDATE_BYTE_COUNT;
}

static inline size_t hpp_candidate_box_index(const hpp_board* board, size_t row, size_t col)
{
    return ((row / board->block_length) * board->block_length) + (col / board->block_length);
}

static inline bool hpp_candidate_budget_consume(hpp_candidate_budget* budget)
{
    if (budget->max_iterations != 0 && budget->iterations >= budget->max_iterations)
    {
        budget->iteration_limit_reached = true;
        return false;
    }

    budget->iterations++;
    return true;
}

static bool hpp_candidate_validate_initial_board(const hpp_board*            board,
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

static hpp_validation_constraints*
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

    for (size_t i = 0; i < source->side_length; ++i)
    {
        memcpy(clone->row_bitvectors[i], source->row_bitvectors[i], HPP_CANDIDATE_BYTE_COUNT);
        memcpy(clone->col_bitvectors[i], source->col_bitvectors[i], HPP_CANDIDATE_BYTE_COUNT);
    }

    const size_t box_side  = source->side_length / source->block_length;
    const size_t box_count = box_side * box_side;
    for (size_t i = 0; i < box_count; ++i)
    {
        memcpy(clone->box_bitvectors[i], source->box_bitvectors[i], HPP_CANDIDATE_BYTE_COUNT);
    }

    return clone;
}

static void hpp_candidate_constraints_copy(hpp_validation_constraints*       destination,
                                           const hpp_validation_constraints* source)
{
    for (size_t i = 0; i < source->side_length; ++i)
    {
        memcpy(destination->row_bitvectors[i], source->row_bitvectors[i], HPP_CANDIDATE_BYTE_COUNT);
        memcpy(destination->col_bitvectors[i], source->col_bitvectors[i], HPP_CANDIDATE_BYTE_COUNT);
    }

    const size_t box_side  = source->side_length / source->block_length;
    const size_t box_count = box_side * box_side;
    for (size_t i = 0; i < box_count; ++i)
    {
        memcpy(destination->box_bitvectors[i], source->box_bitvectors[i], HPP_CANDIDATE_BYTE_COUNT);
    }
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

static size_t
hpp_candidate_compute_cell(hpp_candidate_state* state, size_t cell_index, size_t* single_value)
{
    uint8_t* cell_candidates = hpp_candidate_cell_at(state, cell_index);
    memset(cell_candidates, 0, HPP_CANDIDATE_BYTE_COUNT);

    if (state->board->cells[cell_index] != BOARD_CELL_EMPTY)
    {
        if (single_value != NULL)
        {
            *single_value = BOARD_CELL_EMPTY;
        }
        return 0;
    }

    const size_t   row     = cell_index / state->board->side_length;
    const size_t   col     = cell_index % state->board->side_length;
    const size_t   box_idx = hpp_candidate_box_index(state->board, row, col);
    const uint8_t* row_bv  = state->constraints->row_bitvectors[row];
    const uint8_t* col_bv  = state->constraints->col_bitvectors[col];
    const uint8_t* box_bv  = state->constraints->box_bitvectors[box_idx];

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

    if (single_value != NULL)
    {
        *single_value = (count == 1) ? last_value : BOARD_CELL_EMPTY;
    }

    return count;
}

static bool hpp_candidate_refresh_candidates_and_find_naked_single(hpp_candidate_state* state,
                                                                   size_t*              cell_index,
                                                                   size_t*              value)
{
    *cell_index = SIZE_MAX;
    *value      = BOARD_CELL_EMPTY;

    for (size_t idx = 0; idx < state->board->cell_count; ++idx)
    {
        if (state->board->cells[idx] != BOARD_CELL_EMPTY)
        {
            continue;
        }

        size_t       single_value    = BOARD_CELL_EMPTY;
        const size_t candidate_count = hpp_candidate_compute_cell(state, idx, &single_value);
        if (candidate_count == 0)
        {
            return false;
        }

        if (candidate_count == 1 && *cell_index == SIZE_MAX)
        {
            *cell_index = idx;
            *value      = single_value;
        }
    }

    return true;
}

static bool hpp_candidate_find_hidden_single_in_rows(const hpp_candidate_state* state,
                                                     size_t*                    cell_index,
                                                     size_t*                    value)
{
    const size_t side_length = state->board->side_length;
    for (size_t row = 0; row < side_length; ++row)
    {
        const uint8_t* row_bv = state->constraints->row_bitvectors[row];
        for (size_t candidate_value = 1; candidate_value <= side_length; ++candidate_value)
        {
            if (hpp_candidate_bit_test(row_bv, candidate_value))
            {
                continue;
            }

            size_t candidate_cell  = SIZE_MAX;
            size_t candidate_count = 0;
            for (size_t col = 0; col < side_length; ++col)
            {
                const size_t idx = (row * side_length) + col;
                if (state->board->cells[idx] != BOARD_CELL_EMPTY)
                {
                    continue;
                }

                if (!hpp_candidate_bit_test(hpp_candidate_cell_at_const(state, idx),
                                            candidate_value))
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
    }

    return false;
}

static bool hpp_candidate_find_hidden_single_in_cols(const hpp_candidate_state* state,
                                                     size_t*                    cell_index,
                                                     size_t*                    value)
{
    const size_t side_length = state->board->side_length;
    for (size_t col = 0; col < side_length; ++col)
    {
        const uint8_t* col_bv = state->constraints->col_bitvectors[col];
        for (size_t candidate_value = 1; candidate_value <= side_length; ++candidate_value)
        {
            if (hpp_candidate_bit_test(col_bv, candidate_value))
            {
                continue;
            }

            size_t candidate_cell  = SIZE_MAX;
            size_t candidate_count = 0;
            for (size_t row = 0; row < side_length; ++row)
            {
                const size_t idx = (row * side_length) + col;
                if (state->board->cells[idx] != BOARD_CELL_EMPTY)
                {
                    continue;
                }

                if (!hpp_candidate_bit_test(hpp_candidate_cell_at_const(state, idx),
                                            candidate_value))
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
    }

    return false;
}

static bool hpp_candidate_find_hidden_single_in_boxes(const hpp_candidate_state* state,
                                                      size_t*                    cell_index,
                                                      size_t*                    value)
{
    const size_t side_length  = state->board->side_length;
    const size_t block_length = state->board->block_length;
    const size_t box_side     = side_length / block_length;

    for (size_t box_row = 0; box_row < box_side; ++box_row)
    {
        for (size_t box_col = 0; box_col < box_side; ++box_col)
        {
            const size_t   box_idx = (box_row * box_side) + box_col;
            const uint8_t* box_bv  = state->constraints->box_bitvectors[box_idx];
            for (size_t candidate_value = 1; candidate_value <= side_length; ++candidate_value)
            {
                if (hpp_candidate_bit_test(box_bv, candidate_value))
                {
                    continue;
                }

                size_t candidate_cell  = SIZE_MAX;
                size_t candidate_count = 0;

                const size_t row_start = box_row * block_length;
                const size_t col_start = box_col * block_length;
                for (size_t row_offset = 0; row_offset < block_length; ++row_offset)
                {
                    for (size_t col_offset = 0; col_offset < block_length; ++col_offset)
                    {
                        const size_t row = row_start + row_offset;
                        const size_t col = col_start + col_offset;
                        const size_t idx = (row * side_length) + col;
                        if (state->board->cells[idx] != BOARD_CELL_EMPTY)
                        {
                            continue;
                        }

                        if (!hpp_candidate_bit_test(hpp_candidate_cell_at_const(state, idx),
                                                    candidate_value))
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
        }
    }

    return false;
}

static bool hpp_candidate_find_hidden_single(const hpp_candidate_state* state,
                                             size_t*                    cell_index,
                                             size_t*                    value)
{
    return hpp_candidate_find_hidden_single_in_rows(state, cell_index, value) ||
           hpp_candidate_find_hidden_single_in_cols(state, cell_index, value) ||
           hpp_candidate_find_hidden_single_in_boxes(state, cell_index, value);
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

    state->candidates = calloc(hpp_candidate_buffer_size(state->board), sizeof(uint8_t));
    if (state->candidates == NULL)
    {
        hpp_candidate_state_destroy(state);
        return HPP_CANDIDATE_INIT_ERROR;
    }

    state->remaining_unassigned = hpp_candidate_count_unassigned_cells(state->board);
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
    state->candidates           = NULL;
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

    memcpy(destination->candidates, source->candidates, candidate_buffer_size);
    destination->remaining_unassigned = source->remaining_unassigned;
    return true;
}

bool hpp_candidate_state_assign(hpp_candidate_state*  state,
                                size_t                cell_index,
                                size_t                value,
                                hpp_candidate_budget* budget)
{
    if (!hpp_candidate_budget_consume(budget))
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

    memset(hpp_candidate_cell_at(state, cell_index), 0, HPP_CANDIDATE_BYTE_COUNT);
    if (state->remaining_unassigned > 0)
    {
        state->remaining_unassigned--;
    }

    return true;
}

bool hpp_candidate_state_propagate_singles(hpp_candidate_state* state, hpp_candidate_budget* budget)
{
    while (true)
    {
        size_t naked_single_cell  = SIZE_MAX;
        size_t naked_single_value = BOARD_CELL_EMPTY;
        if (!hpp_candidate_refresh_candidates_and_find_naked_single(
                state, &naked_single_cell, &naked_single_value))
        {
            return false;
        }

        if (naked_single_cell != SIZE_MAX)
        {
            if (!hpp_candidate_state_assign(state, naked_single_cell, naked_single_value, budget))
            {
                return false;
            }

            continue;
        }

        size_t hidden_single_cell  = SIZE_MAX;
        size_t hidden_single_value = BOARD_CELL_EMPTY;
        if (hpp_candidate_find_hidden_single(state, &hidden_single_cell, &hidden_single_value))
        {
            if (!hpp_candidate_state_assign(state, hidden_single_cell, hidden_single_value, budget))
            {
                return false;
            }

            continue;
        }

        return true;
    }
}

hpp_candidate_branch_status hpp_candidate_state_build_branch(hpp_candidate_state*  state,
                                                             hpp_candidate_branch* branch)
{
    if (state->remaining_unassigned == 0)
    {
        return HPP_CANDIDATE_BRANCH_COMPLETE;
    }

    size_t best_cell  = SIZE_MAX;
    size_t best_count = SIZE_MAX;

    for (size_t cell_index = 0; cell_index < state->board->cell_count; ++cell_index)
    {
        if (state->board->cells[cell_index] != BOARD_CELL_EMPTY)
        {
            continue;
        }

        const size_t candidate_count = hpp_candidate_compute_cell(state, cell_index, NULL);
        if (candidate_count == 0)
        {
            return HPP_CANDIDATE_BRANCH_CONFLICT;
        }

        if (candidate_count < best_count)
        {
            best_count = candidate_count;
            best_cell  = cell_index;

            if (best_count == 2)
            {
                break;
            }
        }
    }

    if (best_cell == SIZE_MAX)
    {
        return HPP_CANDIDATE_BRANCH_CONFLICT;
    }

    branch->cell_index  = best_cell;
    branch->value_count = 0;

    const uint8_t* best_candidates = hpp_candidate_cell_at_const(state, best_cell);
    for (size_t value = 1; value <= state->board->side_length; ++value)
    {
        if (!hpp_candidate_bit_test(best_candidates, value))
        {
            continue;
        }

        branch->values[branch->value_count++] = value;
    }

    return (branch->value_count == 0) ? HPP_CANDIDATE_BRANCH_CONFLICT : HPP_CANDIDATE_BRANCH_READY;
}

bool hpp_candidate_state_is_complete(const hpp_candidate_state* state)
{
    return state->remaining_unassigned == 0;
}

void hpp_candidate_state_copy_solution(hpp_candidate_state*       destination,
                                       const hpp_candidate_state* source)
{
    hpp_board_copy(destination->board, source->board);
    hpp_candidate_constraints_copy(destination->constraints, source->constraints);
    memcpy(destination->candidates, source->candidates, hpp_candidate_buffer_size(source->board));
    destination->remaining_unassigned = source->remaining_unassigned;
}
