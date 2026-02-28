#ifndef SOLVER_MOVE_H
#define SOLVER_MOVE_H

#include <stdint.h>

/**
 * Enumeration of possible move kinds in Stochastic Local Search (SLS).
 *
 * ASSIGN: A value is assigned to a cell (has old value for undo semantics).
 * SWAP:   Two values swap positions within the same row or block.
 */
typedef enum MoveKind
{
    MOVE_ASSIGN,
    MOVE_SWAP,
} hpp_move_kind;

/**
 * Decision payload for an ASSIGN move.
 *
 * Represents assigning a new_value to a cell at (row, col),
 * replacing the old_value (useful for undo/visualization).
 */
typedef struct MoveAssign
{
    uint16_t row;
    uint16_t col;
    uint8_t  old_value;
    uint8_t  new_value;
} hpp_move_assign;

/**
 * Decision payload for a SWAP move.
 *
 * Represents swapping values between (row1, col1) and (row2, col2).
 * Consumer can read the actual values from the current board state.
 */
typedef struct MoveSwap
{
    uint16_t row1;
    uint16_t col1;
    uint16_t row2;
    uint16_t col2;
} hpp_move_swap;

/**
 * Union-like structure for a move decision.
 *
 * Use .kind to determine which field is active.
 * Lifetime: immediate, transient. Consumer should copy if retention needed.
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

#endif
