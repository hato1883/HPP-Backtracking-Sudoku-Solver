/**
 * @file parser/parser.h
 * @brief Binary board input/output helpers.
 *
 * On-disk format:
 * - byte 0: `block_length`
 * - byte 1: `side_length`
 * - remaining bytes: flattened cell values in row-major order
 */

#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H

#include "data/board.h"

#include <stdio.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

enum
{
    BUFFER_LIMIT = 255,
    NUMBER_BASE  = 10,
};

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Parse a board from a binary file path.
 *
 * @note Input format is described in this header's file comment.
 * @pre `file_name != NULL` and points to a readable board file.
 * @post On success, returned board must be released with `destroy_board`.
 *
 * @param file_name Path to the input board file.
 * @return Newly allocated board parsed from file.
 *
 * @par Example
 * @code{.c}
 * hpp_board* board = parse_file("board.dat");
 * solve(board, NULL);
 * destroy_board(&board);
 * @endcode
 */
hpp_board* parse_file(const char* file_name);

/**
 * @brief Write a board to any stream (stdout, file, pipe).
 *
 * @note Serialization format is binary and stable for parser/solver pipeline.
 * @pre `file != NULL` and `board != NULL`.
 * @post Stream position is advanced by serialized payload length on success.
 *
 * @param file   Destination stream.
 * @param board  Board instance to serialize.
 * @param silent If non-zero, suppress logging on failures.
 * @return `0` on success, `-1` on I/O or argument errors.
 */
int write_board_to_stream(FILE* file, const hpp_board* board, int silent);

/**
 * @brief Write a board to a binary file path.
 *
 * @pre `filename != NULL` and `board != NULL`.
 * @post On success, file at `filename` contains one serialized board payload.
 * @note Existing files may be overwritten.
 *
 * @param filename Output file path.
 * @param board Board instance to serialize.
 * @return `0` on success, `-1` on failure.
 *
 * @par Example
 * @code{.c}
 * if (write_solution("answer.dat", board) != 0) {
 *     fprintf(stderr, "failed to write solution\n");
 * }
 * @endcode
 */
int write_solution(const char* filename, const hpp_board* board);

#endif /* PARSER_PARSER_H */
