/**
 * @file solver/move.h
 * @brief Lightweight move/event payload types for solver progress reporting.
 */

#ifndef SOLVER_MOVE_H
#define SOLVER_MOVE_H

#include <stdint.h>

/* =========================================================================
 * Move Kinds
 * ========================================================================= */

/** Supported move/event kinds. */
typedef enum MoveKind
{
    MOVE_ASSIGN,
    MOVE_SWAP,
} hpp_move_kind;

/* =========================================================================
 * Payload Types
 * ========================================================================= */

/** Payload for `MOVE_ASSIGN`. */
typedef struct MoveAssign
{
    uint16_t row;
    uint16_t col;
    uint8_t  old_value;
    uint8_t  new_value;
} hpp_move_assign;

/** Payload for `MOVE_SWAP`. */
typedef struct MoveSwap
{
    uint16_t row1;
    uint16_t col1;
    uint16_t row2;
    uint16_t col2;
} hpp_move_swap;

/**
 * @brief Discriminated move payload.
 *
 * Read `kind` first, then inspect the matching union member.
 */
typedef struct Move
{
    hpp_move_kind kind;
    union
    {
        hpp_move_assign assign;
        hpp_move_swap   swap;
    } payload;
} hpp_move;

#endif /* SOLVER_MOVE_H */
