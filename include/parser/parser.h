#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H

#include "data/board.h"

#include <stdio.h>

enum
{
    BUFFER_LIMIT = 255,
    NUMBER_BASE  = 10,
};

hpp_board* parse_file(const char* file_name);

/**
 * @brief Write a board to a FILE* stream (can be stdout or a file).
 *
 * Format: block_size (1 byte) + side (1 byte) + board bytes.
 *
 * @param file   Output stream.
 * @param board  The board to write.
 * @param silent If true, suppress error logging.
 * @return 0 on success, -1 on failure.
 */
int write_board_to_stream(FILE* file, const hpp_board* board, int silent);

/**
 * Write a solved board to a binary file.
 * Format: block_size (1 byte) + side (1 byte) + board bytes
 *
 * @param filename output file path (typically "answer.dat")
 * @param board the solved board to write
 * @return 0 on success, -1 on failure
 */
int write_solution(const char* filename, const hpp_board* board);

#endif
