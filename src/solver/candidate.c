/**
 * @file solver/candidate.c
 * @brief Candidate-state lifecycle, cloning, and shared helpers.
 */

#include "solver/candidate_internal.h"

#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static bool hpp_candidate_validate_initial_board(const hpp_board*            board,
                                                 hpp_validation_constraints* constraints);
static hpp_validation_constraints*
              hpp_candidate_constraints_clone(const hpp_validation_constraints* source);
static void   hpp_candidate_constraints_copy(hpp_validation_constraints*       destination,
                                             const hpp_validation_constraints* source);
static bool   hpp_candidate_build_spatial_orders(hpp_candidate_state* state);
static bool   hpp_candidate_clone_spatial_orders(const hpp_candidate_state* source,
                                                 hpp_candidate_state*       destination);
static void   hpp_candidate_destroy_spatial_orders(hpp_candidate_state* state);
static size_t hpp_candidate_buffer_size(const hpp_board* board);
static size_t hpp_candidate_box_index(const hpp_board* board, size_t row, size_t col);
static size_t hpp_candidate_count_unassigned_cells(const hpp_board* board);
static void   hpp_candidate_modified_reset(hpp_candidate_state* state);
static size_t hpp_candidate_find_single_value_for_cell(const hpp_candidate_state* state,
                                                       size_t                     cell_index);
static size_t
hpp_candidate_compute_cell(hpp_candidate_state* state, size_t cell_index, size_t* single_value);
static bool hpp_candidate_initialize_cache_and_work_stack(hpp_candidate_state* state);

/* =========================================================================
 * Public API
 * ========================================================================= */

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

/* =========================================================================
 * Internal Helpers
 * ========================================================================= */

/**
 * @brief Validate board shape and seed constraints from initial board values.
 *
 * @note This is the single gate that rejects unsupported board dimensions
 *       before candidate buffers are populated.
 * @pre `board != NULL` and `constraints != NULL`.
 * @post On success, `constraints` reflects all assigned board values.
 *
 * @param board Source board to validate.
 * @param constraints Pre-allocated constraint object to initialize.
 * @return `true` when board invariants hold and constraints init succeeds.
 */
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

/**
 * @brief Deep-clone validation constraint bitvectors.
 *
 * @note Allocates a fresh constraints object and copies row/col/box slabs.
 * @pre `source != NULL`.
 * @post Returned object is independent from `source`.
 *
 * @param source Constraint snapshot to clone.
 * @return Newly allocated clone, or `NULL` on allocation failure.
 */
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

    const size_t side_bytes     = source->side_length * HPP_CANDIDATE_CELL_BYTES;
    const size_t boxes_per_side = source->side_length / source->block_length;
    const size_t box_count      = boxes_per_side * boxes_per_side;
    const size_t box_bytes      = box_count * HPP_CANDIDATE_CELL_BYTES;

    memcpy(clone->row_bitvectors, source->row_bitvectors, side_bytes);
    memcpy(clone->col_bitvectors, source->col_bitvectors, side_bytes);
    memcpy(clone->box_bitvectors, source->box_bitvectors, box_bytes);

    return clone;
}

/**
 * @brief Copy constraint bitvector slabs between compatible objects.
 *
 * @note Caller is responsible for ensuring matching geometry
 *       (`side_length`/`block_length`) between source and destination.
 * @pre `destination != NULL` and `source != NULL`.
 * @post Destination row/col/box bitvectors exactly match source.
 *
 * @param destination Constraint object receiving copied contents.
 * @param source Constraint object supplying copied contents.
 */
static void hpp_candidate_constraints_copy(hpp_validation_constraints*       destination,
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

/**
 * @brief Build cached row/column/box traversal orders for locality.
 *
 * @note These arrays are reused by propagation to avoid recomputing peer
 *       index patterns on hot paths.
 * @pre `state != NULL` and `state->board != NULL`.
 * @post On success, all three order arrays are allocated and populated.
 *
 * @param state Candidate state receiving traversal caches.
 * @return `true` when all allocations/population steps succeed.
 */
static bool hpp_candidate_build_spatial_orders(hpp_candidate_state* state)
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

/**
 * @brief Clone precomputed spatial traversal arrays.
 *
 * @note Performs full deep copy so cloned states can mutate independently.
 * @pre `source != NULL` and `destination != NULL`.
 * @post On success, destination order arrays match source arrays.
 *
 * @param source Candidate state providing cached orders.
 * @param destination Candidate state receiving cloned orders.
 * @return `true` when all allocations and copies succeed.
 */
static bool hpp_candidate_clone_spatial_orders(const hpp_candidate_state* source,
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

/**
 * @brief Release spatial traversal cache arrays.
 *
 * @note Safe to call on partially initialized states.
 * @pre `state != NULL`.
 * @post `row_cell_order`, `col_cell_order`, and `box_cell_order` are NULL.
 *
 * @param state Candidate state whose order caches are destroyed.
 */
static void hpp_candidate_destroy_spatial_orders(hpp_candidate_state* state)
{
    free(state->row_cell_order);
    free(state->col_cell_order);
    free(state->box_cell_order);

    state->row_cell_order = NULL;
    state->col_cell_order = NULL;
    state->box_cell_order = NULL;
}

/**
 * @brief Return total bytes required for all per-cell candidate domains.
 *
 * @note This keeps allocation/copy byte-count logic centralized in one place.
 * @pre `board != NULL`.
 * @post Board state is unchanged.
 *
 * @param board Board geometry source.
 * @return Byte size required for candidate slab storage.
 */
static size_t hpp_candidate_buffer_size(const hpp_board* board)
{
    return board->cell_count * HPP_CANDIDATE_CELL_BYTES;
}

/**
 * @brief Compute box index for one `(row, col)` cell coordinate.
 *
 * @note Mapping is row-major over box coordinates.
 * @pre `board != NULL`, `board->block_length > 0`, and indices are in range.
 * @post Board state is unchanged.
 *
 * @param board Board geometry source.
 * @param row Cell row index.
 * @param col Cell column index.
 * @return Box index containing `(row, col)`.
 */
static size_t hpp_candidate_box_index(const hpp_board* board, size_t row, size_t col)
{
    return ((row / board->block_length) * board->block_length) + (col / board->block_length);
}

/**
 * @brief Count currently unassigned cells on a board snapshot.
 *
 * @note Used during initialization to seed `remaining_unassigned`.
 * @pre `board != NULL` and `board->cells != NULL`.
 * @post Board contents are unchanged.
 *
 * @param board Board snapshot to scan.
 * @return Number of cells equal to `BOARD_CELL_EMPTY`.
 */
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

/**
 * @brief Push a cell index onto the modified-work stack (deduplicated).
 *
 * @note `modified_in_stack` prevents duplicate pushes for the same cell.
 * @pre `state != NULL` and `cell_index` is within board range.
 * @post On success, `cell_index` is marked pending for propagation work.
 *
 * @param state Candidate state containing work-stack buffers.
 * @param cell_index Cell index to enqueue.
 * @return `false` on invalid state/index or stack-capacity failure.
 */
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

/**
 * @brief Pop one cell index from the modified-work stack.
 *
 * @note Stack is LIFO to keep recently-changed cells hot in cache.
 * @pre `state != NULL` and `cell_index != NULL`.
 * @post On success, popped cell is removed from in-stack dedup set.
 *
 * @param state Candidate state containing work-stack buffers.
 * @param cell_index Output location for popped cell index.
 * @return `true` when one pending cell was popped.
 */
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

/**
 * @brief Clear all pending modified-work entries.
 *
 * @note Resets both stack depth and dedup bitmap in one operation.
 * @pre `state != NULL` and stack buffers are allocated.
 * @post No cells remain pending in propagation work stack.
 *
 * @param state Candidate state whose work stack is reset.
 */
static void hpp_candidate_modified_reset(hpp_candidate_state* state)
{
    state->modified_count = 0;
    memset(state->modified_in_stack, 0, state->board->cell_count * sizeof(uint8_t));
}

/**
 * @brief Return first candidate value present in a cell domain.
 *
 * @note Caller typically uses this only when domain cardinality is known to
 *       be exactly one.
 * @pre `state != NULL` and `cell_index` is valid.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state to inspect.
 * @param cell_index Cell index whose domain is queried.
 * @return First present candidate value, or `BOARD_CELL_EMPTY` if none.
 */
static size_t hpp_candidate_find_single_value_for_cell(const hpp_candidate_state* state,
                                                       size_t                     cell_index)
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

/**
 * @brief Resolve singleton value for AC-3 style propagation checks.
 *
 * @note Prefers concrete board assignment when already set; otherwise inspects
 *       candidate domain when count indicates singleton.
 * @pre `state != NULL` and `cell_index` is valid.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state to inspect.
 * @param cell_index Cell index to query.
 * @return Singleton value, or `BOARD_CELL_EMPTY` when unresolved.
 */
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

/**
 * @brief Recompute candidate domain for one cell from constraint bitvectors.
 *
 * @note Clears old domain first, then rebuilds by rejecting values present in
 *       row/column/box occupancy caches.
 * @pre `state != NULL` and `cell_index` is within board range.
 * @post `candidate_counts[cell_index]` and domain bits are synchronized.
 *
 * @param state Candidate state to mutate.
 * @param cell_index Cell index to recompute.
 * @param single_value Optional output for resolved singleton value.
 * @return Candidate count computed for the target cell.
 */
static size_t
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

        hpp_bitvector_set(cell_candidates, value);
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

/**
 * @brief Initialize candidate cache and seed work stack for propagation.
 *
 * @note Every cell is enqueued once so propagation sees the complete initial
 *       board context (assigned and unassigned cells alike).
 * @pre `state != NULL` and all cache buffers are allocated.
 * @post Candidate domains/counts are initialized and work stack is populated.
 *
 * @param state Candidate state to initialize.
 * @return `true` when initialization and queue seeding complete successfully.
 */
static bool hpp_candidate_initialize_cache_and_work_stack(hpp_candidate_state* state)
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
